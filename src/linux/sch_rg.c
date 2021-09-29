// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Roundsgate queueing discipline
 *
 *  Copyright (C) 2021 Toke Høiland-Jørgensen <toke@redhat.com>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <net/netlink.h>
#include <net/pkt_sched.h>


struct rg_queue {
       struct sk_buff    *head;
       struct sk_buff    *tail;
};

struct rg_sched_data {
       struct rg_queue queues[2];      /* the queues being pointed to below */
       struct rg_queue *input_queue;
       struct rg_queue *output_queue;
       struct qdisc_watchdog watchdog;
       ktime_t next_q_swap;
       u64 tick_interval;
};


static inline void queue_add(struct rg_queue *queue, struct sk_buff *skb)
{
       if (queue->head == NULL)
               queue->head = skb;
       else
               queue->tail->next = skb;
       queue->tail = skb;
       skb->next = NULL;
}


static int rg_enqueue(struct sk_buff *skb, struct Qdisc *sch,
                     struct sk_buff **to_free)
{
       struct rg_sched_data *q = qdisc_priv(sch);

       /* Orphan the skb to disable TSQ */
       skb_orphan(skb);
       queue_add(q->input_queue, skb);
       qdisc_qstats_backlog_inc(sch, skb);

       return NET_XMIT_SUCCESS;
}

static inline struct sk_buff *dequeue_head(struct rg_queue *queue)
{
       struct sk_buff *skb = queue->head;

       if (!skb)
               return NULL;

       queue->head = skb->next;
       skb_mark_not_on_list(skb);
       return skb;
}

static void rg_queue_drop_all(struct rg_queue *queue, int *n, int *len)
{
       struct sk_buff *head = queue->head;
       queue->head = NULL;

       while (head) {
               struct sk_buff *next = head->next;
               *n += 1;
               *len += qdisc_pkt_len(head);
               kfree_skb(head);
               head = next;
       }
}

static struct sk_buff *rg_dequeue(struct Qdisc *sch)
{
       struct rg_sched_data *q = qdisc_priv(sch);
       ktime_t now = ktime_get();
       struct sk_buff *skb;

       /* Swap the queues every interval; the qdisc watchdog makes sure this
        * gets scheduled every interval even if the qdisc is otherwise empty
        */
       if (ktime_after(now, q->next_q_swap)) {
               struct rg_queue *tmp;
               int dropped_p = 0, dropped_b = 0;

               /* swap queues, flushing anything that's left in the old output
                * queue
                */
               tmp = q->output_queue;
               q->output_queue = q->input_queue;
               rg_queue_drop_all(tmp, &dropped_p, &dropped_b);
               qdisc_tree_reduce_backlog(sch, dropped_p, dropped_b);
               sch->qstats.drops += dropped_p;
               q->input_queue = tmp;

               /* reschedule the watchdog to make sure we get the next swap at
                * the right time
                */
               q->next_q_swap = ktime_add_ns(now, q->tick_interval);
               qdisc_watchdog_schedule_ns(&q->watchdog, ktime_to_ns(q->next_q_swap));
       }

       skb = dequeue_head(q->output_queue);
       if (!skb)
               return NULL;

       qdisc_bstats_update(sch, skb);
       qdisc_qstats_backlog_dec(sch, skb);
       /* We cant call qdisc_tree_reduce_backlog() if our qlen is 0,
        * or HTB crashes. Defer it for next round.
        */
       if (sch->q.qlen)
               qdisc_tree_reduce_backlog(sch, 1, qdisc_pkt_len(skb));
       return skb;
}

#define NS_PER_MS 1000000
static int rg_init(struct Qdisc *sch, struct nlattr *opt,
                        struct netlink_ext_ack *extack)
{
       struct rg_sched_data *q = qdisc_priv(sch);
       ktime_t now = ktime_get();

       /* FIXME: The tick interval should be configurable */
       q->tick_interval = 100 * NS_PER_MS;
       q->next_q_swap = ktime_add_ns(now, q->tick_interval);
       q->input_queue = &q->queues[0];
       q->output_queue = &q->queues[1];
       qdisc_watchdog_init(&q->watchdog, sch);
       qdisc_watchdog_schedule_ns(&q->watchdog, ktime_to_ns(q->next_q_swap));
       sch->flags &= ~TCQ_F_CAN_BYPASS;
       return 0;
}

static void rg_queue_purge(struct rg_queue *queue)
{
       rtnl_kfree_skbs(queue->head, queue->tail);
       queue->head = NULL;
}

static void rg_reset(struct Qdisc *sch)
{
       struct rg_sched_data *q = qdisc_priv(sch);

       rg_queue_purge(&q->queues[0]);
       rg_queue_purge(&q->queues[1]);

       sch->q.qlen = 0;
       sch->qstats.backlog = 0;
}

static void rg_destroy(struct Qdisc *sch)
{
}

static int rg_change(struct Qdisc *sch, struct nlattr *opt,
                    struct netlink_ext_ack *extack)
{
       /* FIXME: implement this */
       return -EOPNOTSUPP;
}

static int rg_dump(struct Qdisc *sch, struct sk_buff *skb)
{
       return 0;
}

static int rg_dump_stats(struct Qdisc *sch, struct gnet_dump *d)
{
       u32 st = 0;
       return gnet_stats_copy_app(d, &st, sizeof(st));
}

static struct Qdisc_ops rg_qdisc_ops __read_mostly = {
       .id             =       "rg",
       .priv_size      =       sizeof(struct rg_sched_data),
       .enqueue        =       rg_enqueue,
       .dequeue        =       rg_dequeue,
       .peek           =       qdisc_peek_dequeued,
       .init           =       rg_init,
       .reset          =       rg_reset,
       .destroy        =       rg_destroy,
       .change         =       rg_change,
       .dump           =       rg_dump,
       .dump_stats =   rg_dump_stats,
       .owner          =       THIS_MODULE,
};

static int __init rg_module_init(void)
{
       return register_qdisc(&rg_qdisc_ops);
}

static void __exit rg_module_exit(void)
{
       unregister_qdisc(&rg_qdisc_ops);
}

module_init(rg_module_init)
module_exit(rg_module_exit)
MODULE_AUTHOR("Toke Høiland-Jørgensen");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("rg queueing discipline");