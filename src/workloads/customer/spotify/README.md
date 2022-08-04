To run the test and parse the results:
1. Replace username and password with the database credential
2. Run it!

```shell
export ATLAS_USER=username
export ATLAS_PWD=password
mkdir ../myresults

./run-genny workload -u "mongodb+srv://$ATLAS_USER:$ATLAS_PWD@spotify.y29cz.mongodb.net/?retryWrites=true&w=majority" ./src/workloads/spotify_analytics.yml
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TopArtistQuery.ftdc > ../myresults/TopArtistQuery_10.json
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TopGenreQuery.ftdc > ../myresults/TopGenreQuery_10.json
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TrackTasteQuery.ftdc > ../myresults/TrackTasteQuery_10.json

./run-genny workload -u "mongodb+srv://$ATLAS_USER:$ATLAS_PWD@spotify.y29cz.mongodb.net/?retryWrites=true&w=majority" ./src/workloads/spotify_50.yml
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TopArtistQuery.ftdc > ../myresults/TopArtistQuery_50.json
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TopGenreQuery.ftdc > ../myresults/TopGenreQuery_50.json
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TrackTasteQuery.ftdc > ../myresults/TrackTasteQuery_50.json

./run-genny workload -u "mongodb+srv://$ATLAS_USER:$ATLAS_PWD@spotify.y29cz.mongodb.net/?retryWrites=true&w=majority" ./src/workloads/spotify_100.yml
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TopArtistQuery.ftdc > ../myresults/TopArtistQuery_100.json
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TopGenreQuery.ftdc > ../myresults/TopGenreQuery_100.json
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TrackTasteQuery.ftdc > ../myresults/TrackTasteQuery_100.json

./run-genny workload -u "mongodb+srv://$ATLAS_USER:$ATLAS_PWD@spotify.y29cz.mongodb.net/?retryWrites=true&w=majority" ./src/workloads/spotify_500.yml
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TopArtistQuery.ftdc > ../myresults/TopArtistQuery_500.json
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TopGenreQuery.ftdc > ../myresults/TopGenreQuery_500.json
./build/curator/curator ftdc export json --input ./build/WorkloadOutput/CedarMetrics/AnalyticsQueries.TrackTasteQuery.ftdc > ../myresults/TrackTasteQuery_500.json
```