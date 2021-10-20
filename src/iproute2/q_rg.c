// SPDX-License-Identifier: GPL-2.0-only

#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "tc_util.h"

static void explain(void)
{
	fprintf(stderr,
		"Usage: ... rg [ interval ms ]\n");
}

static int rg_parse_opt(struct qdisc_util *qu, int argc, char **argv,
			 struct nlmsghdr *n, const char *dev)
{
	unsigned int interval = 0;
	struct rtattr *tail;

	while (argc > 0) {
		if (strcmp(*argv, "interval") == 0) {
			NEXT_ARG();
			if (get_unsigned(&interval, *argv, 0)) {
				fprintf(stderr, "Illegal \"interval\"\n");
				return -1;
			}
		} else {
			fprintf(stderr, "What is \"%s\"?\n", *argv);
			explain();
			return -1;
		}

		argc--;
		argv++;
	}

	tail = addattr_nest(n, 1024, TCA_OPTIONS | NLA_F_NESTED);
	if (interval)
		addattr_l(n, 1024, TCA_RG_INTERVAL, &interval, sizeof(interval));
	addattr_nest_end(n, tail);

	return 0;
}

static int rg_print_opt(struct qdisc_util *qu, FILE *f, struct rtattr *opt)
{
	struct rtattr *tb[TCA_RG_MAX + 1];
	unsigned int interval;

	if (opt == NULL)
		return 0;

	parse_rtattr_nested(tb, TCA_RG_MAX, opt);

	if (tb[TCA_RG_INTERVAL] &&
	    RTA_PAYLOAD(tb[TCA_RG_INTERVAL]) >= sizeof(__u32)) {
		interval = rta_getattr_u32(tb[TCA_RG_INTERVAL]);
		print_uint(PRINT_ANY, "interval", "interval %ums ", interval/1000000);
	}

	return 0;
}

static int rg_print_xstats(struct qdisc_util *qu, FILE *f,
			    struct rtattr *xstats)
{
	struct tc_rg_xstats _st = {}, *st;

	SPRINT_BUF(b1);

	if (xstats == NULL)
		return 0;

	st = RTA_DATA(xstats);
	if (RTA_PAYLOAD(xstats) < sizeof(*st)) {
		memcpy(&_st, st, RTA_PAYLOAD(xstats));
		st = &_st;
	}

	print_uint(PRINT_JSON, "delay", NULL, st->delay);
	print_string(PRINT_FP, NULL, "  delay %s", sprint_time(st->delay, b1));

	return 0;
}

struct qdisc_util rg_qdisc_util = {
	.id		= "rg",
	.parse_qopt	= rg_parse_opt,
	.print_qopt	= rg_print_opt,
	.print_xstats	= rg_print_xstats
};
