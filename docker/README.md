<img src="https://user-images.githubusercontent.com/119094/67700512-75458380-f984-11e9-9b81-668ea220b9fa.jpg" align="right" height="282" width="320">

Genny and Docker
=====

Docker file images:

1. amazon2       - Dockerfile / Dockerfile.amazon2 **(default)**
2. Ubuntu 18.04  - Dockerfile.1804
2. Ubuntu 20.04  - Dockerfile.2004
2. Centos7       - Dockerfile.centos7

Docker compose file:

1. docker-compose.yml - docker compose configuration.
2. client-variables.env - client environment configuration.

Misc file:

1. download-client-metrics.sh - shell script to copy WorkloadOutput from containers.
2. README.md - this file.

Create an amazon2 docker container.
```bash
$> docker build -t  genny/amazon:2 ./ -f docker/Dockerfile --no-cache --rm=true
```

When developing, remove the no-cache and rm flags for faster builds:
```bash
$> docker build -t  genny/amazon:2 ./ -f docker/Dockerfile
```


Run a shell in the newly created docker image.
```bash
$> docker run   -it genny/amazon:2
```

Shutdown the previous containers and then run 270 new clients, on completion copy out the workload data.
```bash
$> docker-compose -f docker/docker-compose.yml down --remove-orphans
$> docker-compose -f docker/docker-compose.yml up --scale genny=270 && docker/download-client-metrics.sh
```

Run 270 new clients, don't wait do a ps and then follow the logs.
```bash
$> docker-compose -f docker/docker-compose.yml up -d  --scale genny=270
$> docker-compose -f docker/docker-compose.yml ps
$> docker-compose -f docker/docker-compose.yml logs -f
```

Run a different docker image:

```bash
GENNY_DOCKER_IMAGE=genny/ubuntu:1804 docker-compose -f docker/docker-compose.yml up -d  --scale genny=270
```

Other Flavours
=====

Create other docker container.
```bash
$> docker build -t  genny/ubuntu:1804 ./ -f docker/Dockerfile.1804 --no-cache --rm=true
$> docker build -t  genny/ubuntu:2004 ./ -f docker/Dockerfile.2004 --no-cache --rm=true
$> docker build -t  genny/centos:7 ./ -f docker/Dockerfile.centos7 --no-cache --rm=true
```


Install Docker
=====

If you run out of disk space when creating docker images, then set a new docker location:

```bash
  $> cat << EOF | sudo tee /etc/docker/daemon.json
{
  "data-root": "/data/docker"
}
EOF
  $> sudo service docker restart
  $> yes | sudo docker system prune -a
```
