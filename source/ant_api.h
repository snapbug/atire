/*
 * ANT_API.H
 * ---------
 */

#ifndef __ANT_API_H__
#define __ANT_API_H__

#include "ant_params.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef void ANT;

ANT *ant_easy_init();

void ant_setup(ANT *ant);

ANT_ANT_params *ant_params(ANT *ant);

void ant_post_processing_stats_init(ANT *ant);

long long ant_search(ANT *ant, char *query, long topic_id = -1);

double ant_cal_map(ANT *ant);

void ant_stat(ANT *ant);

void ant_free(ANT *ant);

#ifdef  __cplusplus
}
#endif

#endif /* __ANT_API_H__ */