const testDB = db.getSiblingDB("cpu");

jsTestLog("Starting test.js with db = " + testDB.getName());

for (let i = 0; i < 10; ++i) {
	const res = testDB.point_data.aggregate([
	    {
		$match: {
		    measurement: "cpu",
		    "tags.hostname": {$in: [
			"host_0", "host_1", "host_2", "host_3", "host_4", "host_5", "host_6", "host_7"
		    ]},
		    time: {$gte: ISODate("2016-01-04T21:49:27Z"), $lt: ISODate("2016-01-04T22:49:27Z")}
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
	]).toArray();
	jsTestLog(res.tojson());
}
