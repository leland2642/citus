/*-------------------------------------------------------------------------
 *
 * insert_select_planner.c
 *
 * Planning logic for INSERT..SELECT.
 *
 * Copyright (c) 2017, Citus Data, Inc.
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "distributed/insert_select_planner.h"
#include "distributed/multi_executor.h"
#include "distributed/multi_physical_planner.h"
#include "distributed/multi_router_planner.h"
#include "distributed/pg_dist_partition.h"
#include "nodes/parsenodes.h"
#include "optimizer/planner.h"
#include "parser/parsetree.h"


/*
 * CreatteCoordinatorInsertSelectPlan creates a query plan for a SELECT into a
 * distributed table.
 */
MultiPlan *
CreateCoordinatorInsertSelectPlan(Query *parse)
{
	Query *insertSelectQuery = copyObject(parse);

	RangeTblRef *reference = linitial(insertSelectQuery->jointree->fromlist);
	RangeTblEntry *subqueryRte = rt_fetch(reference->rtindex,
										  insertSelectQuery->rtable);
	RangeTblEntry *insertRte = rt_fetch(insertSelectQuery->resultRelation,
										insertSelectQuery->rtable);
	Oid targetRelationId = insertRte->relid;

	Query *subquery = (Query *) subqueryRte->subquery;
	MultiPlan *multiPlan = CitusMakeNode(MultiPlan);
	multiPlan->operation = CMD_INSERT;

	if (list_length(insertSelectQuery->cteList) > 0)
	{
		multiPlan->planningError =
			DeferredError(ERRCODE_FEATURE_NOT_SUPPORTED,
						  "CTEs are not supported in INSERT ... SELECT via coordinator",
						  NULL, NULL);

		return multiPlan;
	}

	if (list_length(insertSelectQuery->returningList) > 0)
	{
		multiPlan->planningError =
			DeferredError(ERRCODE_FEATURE_NOT_SUPPORTED,
						  "RETURNING is not supported in INSERT ... SELECT via coordinator",
						  NULL, NULL);

		return multiPlan;
	}

	if (insertSelectQuery->onConflict)
	{
		multiPlan->planningError =
			DeferredError(ERRCODE_FEATURE_NOT_SUPPORTED,
						  "ON CONFLICT is not supported in INSERT ... SELECT via coordinator",
						  NULL, NULL);
	}

	if (PartitionMethod(targetRelationId) == DISTRIBUTE_BY_APPEND)
	{
		multiPlan->planningError =
			DeferredError(ERRCODE_FEATURE_NOT_SUPPORTED,
						  "INSERT ... SELECT into an append-distributed table is not supported",
						  NULL, NULL);
	}


	ReorderInsertSelectTargetLists(insertSelectQuery, insertRte, subqueryRte);

	multiPlan->insertSelectQuery = subquery;
	multiPlan->insertTargetList = insertSelectQuery->targetList;
	multiPlan->targetRelationId = targetRelationId;

	return multiPlan;
}
