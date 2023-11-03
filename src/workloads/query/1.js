{
    "t": {"$date": "2023-11-03T14:57:28.414+00:00"}, "s": "I", "c": "COMMAND", "id": 51803,
        "ctx": "conn7", "msg": "Slow query", "attr": {
            "type": "command",
            "ns": "test.Collection0",
            "appName": "Genny",
            "command": {
                "getMore": 5830736535248202730,
                "collection": "Collection0",
                "$db": "test",
                "lsid": {"id": {"$uuid": "7e937d21-55a5-4006-b777-3f60f97fd4bc"}}
            },
            "originatingCommand": {
                "aggregate": "Collection0",
                "pipeline": [{
                    "$densify": {
                        "field": "hours",
                        "partitionByFields": ["partitionKey"],
                        "range": {"bounds": "full", "step": 3, "unit": "hour"}
                    }
                }],
                "cursor": {},
                "$db": "test",
                "lsid": {"id": {"$uuid": "7e937d21-55a5-4006-b777-3f60f97fd4bc"}}
            },
            "planSummary": "IXSCAN { hours: 1 }",
            "cursorid": 5830736535248202730,
            "keysExamined": 0,
            "docsExamined": 0,
            "fromPlanCache": true,
            "nBatches": 1,
            "numYields": 0,
            "nreturned": 322215,
            "queryHash": "414A328E",
            "planCacheKey": "C34F9A6F",
            "queryFramework": "sbe",
            "reslen": 16777265,
            "locks": {},
            "cpuNanos": 391946806,
            "remote": "127.0.0.1:33768",
            "protocol": "op_msg",
            "durationMillis": 391
        }
}
