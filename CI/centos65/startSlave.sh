#!/bin/sh
while [ true ]; do 
	curl -o slave.jar http://${MASTER_PORT_8080_TCP_ADDR}:${MASTER_PORT_8080_TCP_PORT}/jnlpJars/slave.jar
	java -jar slave.jar -jnlpUrl http://${MASTER_PORT_8080_TCP_ADDR}:${MASTER_PORT_8080_TCP_PORT}/computer/centos65/slave-agent.jnlp
	sleep 1
done
