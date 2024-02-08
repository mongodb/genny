db.point_data.explain("executionStats").aggregate([
    {
        $match: {
            measurement: "cpu",
            "tags.hostname": {$in: ["host_0"]},
            time: {$gte: ISODate("2016-01-04T21:49:27Z"), $lt: ISODate("2016-01-05T09:49:27Z")}
        }
    },
    {
        $group: {
            _id: {$dateTrunc: {"date": "$time", "unit": "minute"}},
            max_usage_user: {$max: "$usage_user"},
            max_usage_guest: {$max: "$usage_guest"},
            max_usage_system: {$max: "$usage_system"},
            max_usage_iowait: {$max: "$usage_iowait"},
            max_usage_irq: {$max: "$usage_irq"}
        }
    },
    {$sort: {_id: 1}}
]);

db.point_data.aggregate([
    {
        $match: {
            measurement: "cpu",
            "tags.hostname": {$in: ["host_0"]},
            time: {$gte: ISODate("2016-01-04T21:49:27Z"), $lt: ISODate("2016-01-05T09:49:27Z")}
        }
    },
    {
        $group: {
            _id: {$dateTrunc: {"date": "$time", "unit": "minute"}},
            max_usage_user: {$max: "$usage_user"},
            max_usage_guest: {$max: "$usage_guest"},
            max_usage_system: {$max: "$usage_system"},
            max_usage_iowait: {$max: "$usage_iowait"},
            max_usage_irq: {$max: "$usage_irq"}
        }
    },
    {$sort: {_id: 1}}
]);
