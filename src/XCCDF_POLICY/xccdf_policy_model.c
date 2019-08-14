/*
 * Copyright 2016 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Authors:
 *    Šimon Lukašík
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common/list.h"
#include "public/oscap_text.h"
#include "public/xccdf_benchmark.h"
#include "public/xccdf_policy.h"
#include "xccdf_policy_model_priv.h"
#include "xccdf_policy_priv.h"
#include "XCCDF/item.h"
#include "XCCDF/helpers.h"

struct xccdf_policy *xccdf_policy_model_get_existing_policy_by_id(struct xccdf_policy_model *policy_model, const char *profile_id)
{
	struct xccdf_policy_iterator *policy_it = xccdf_policy_model_get_policies(policy_model);
	while (xccdf_policy_iterator_has_more(policy_it)) {
		struct xccdf_policy *policy = xccdf_policy_iterator_next(policy_it);
		if (oscap_streq(xccdf_policy_get_id(policy), profile_id)) {
			xccdf_policy_iterator_free(policy_it);
			return policy;
		}
        }
	xccdf_policy_iterator_free(policy_it);
	return NULL;
}

static void _add_selectors_for_all_xccdf_items(struct xccdf_profile *profile, struct xccdf_item *item)
{
	struct xccdf_item_iterator *children = NULL;
	if (xccdf_item_get_type(item) == XCCDF_BENCHMARK) {
		children = xccdf_benchmark_get_content(XBENCHMARK(item));
	} else if (xccdf_item_get_type(item) == XCCDF_GROUP) {
		children = xccdf_group_get_content(XGROUP(item));
	}

	if (xccdf_item_get_type(item) == XCCDF_RULE ||
		xccdf_item_get_type(item) == XCCDF_GROUP)
	{
		struct xccdf_select *select = xccdf_select_new();
		xccdf_select_set_item(select, xccdf_item_get_id(item));
		xccdf_select_set_selected(select, true);
		xccdf_profile_add_select(profile, select);
	}

	if (children) {
		while (xccdf_item_iterator_has_more(children)) {
			struct xccdf_item *current = xccdf_item_iterator_next(children);
			_add_selectors_for_all_xccdf_items(profile, current);
		}
		xccdf_item_iterator_free(children);
	}
}

struct xccdf_policy *xccdf_policy_model_create_policy_by_id(struct xccdf_policy_model *policy_model, const char *id)
{
	struct xccdf_profile *profile = NULL;
	struct xccdf_tailoring *tailoring = xccdf_policy_model_get_tailoring(policy_model);

	// Tailoring profiles take precedence over Benchmark profiles.
	if (tailoring) {
		profile = xccdf_tailoring_get_profile_by_id(tailoring, id);
	}

	// The (default) and (all) profiles are de-facto owned by the xccdf_policy
	// and will be freed by it when it's freed. See xccdf_policy_free.

	if (!profile) {
		if (id == NULL) {
			profile = xccdf_profile_new();
			xccdf_profile_set_id(profile, NULL);
			struct oscap_text * title = oscap_text_new();
			oscap_text_set_text(title, "No profile (default benchmark)");
			oscap_text_set_lang(title, "en");
			xccdf_profile_add_title(profile, title);
		} else {
			struct xccdf_benchmark *benchmark = xccdf_policy_model_get_benchmark(policy_model);
			if (benchmark == NULL) {
				assert(benchmark != NULL);
				return NULL;
			}

			if (strcmp(id, "(all)") == 0) {
				profile = xccdf_profile_new();
				xccdf_profile_set_id(profile, "(all)");
				struct oscap_text *title = oscap_text_new();
				oscap_text_set_text(title, "(all) profile (all rules selected)");
				oscap_text_set_lang(title, "en");
				xccdf_profile_add_title(profile, title);

				_add_selectors_for_all_xccdf_items(profile, XITEM(benchmark));
			} else {
				profile = xccdf_benchmark_get_profile_by_id(benchmark, id);
				if (profile == NULL)
					return NULL;
			}
		}
	}

	return xccdf_policy_new(policy_model, profile);
}

static int _xccdf_policy_model_create_policy_if_useful(struct xccdf_policy_model *policy_model, const char *profile_id)
{
	if (xccdf_policy_model_get_existing_policy_by_id(policy_model, profile_id) == NULL) {
		struct xccdf_policy *policy = xccdf_policy_model_create_policy_by_id(policy_model, profile_id);
		if (policy == NULL) {
			return -1;
		}
		if (xccdf_policy_get_selected_rules_count(policy) > 0) {
			xccdf_policy_model_add_policy(policy_model, policy);
		}
	}
	return 0;
}

static int _xccdf_policy_model_create_policies_if_useful(struct xccdf_policy_model *policy_model, struct xccdf_profile_iterator *profit)
{
	while (xccdf_profile_iterator_has_more(profit)) {
		struct xccdf_profile *profile = xccdf_profile_iterator_next(profit);
		const char *profile_id = xccdf_profile_get_id(profile);
		if (_xccdf_policy_model_create_policy_if_useful(policy_model, profile_id) != 0) {
			xccdf_profile_iterator_free(profit);
			return -1;
		}
	}
	xccdf_profile_iterator_free(profit);
	return 0;
}

int xccdf_policy_model_build_all_useful_policies(struct xccdf_policy_model *policy_model)
{
	if (_xccdf_policy_model_create_policy_if_useful(policy_model, NULL) != 0) {
		return -1;
	}
	struct xccdf_tailoring *tailoring = xccdf_policy_model_get_tailoring(policy_model);
	if (tailoring != NULL) {
		struct xccdf_profile_iterator *profit = xccdf_tailoring_get_profiles(tailoring);
		if (_xccdf_policy_model_create_policies_if_useful(policy_model, profit) != 0) {
			return -1;
		}
	}

	struct xccdf_benchmark *benchmark = xccdf_policy_model_get_benchmark(policy_model);
	if (benchmark == NULL) {
		return -1;
	}
	struct xccdf_profile_iterator *profit = xccdf_benchmark_get_profiles(benchmark);
	if (_xccdf_policy_model_create_policies_if_useful(policy_model, profit) != 0) {
		return -1;
	}
	return 0;
}
