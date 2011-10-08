#! /bin/bash -f
# This script calls the necessary tools to do one pass of dynamic pipe reassignment

if [ "$#" != "5" ]
then
echo "Usage: $0 <mdb> <logs> <tactic> <port> <logdirname>"
exit 1
fi

if [ -f counter ]; then
    expr `cat counter; rm counter` + 1 > counter
else
    echo 0 > counter
fi

BINDIR=${MN_HOME}/bin
MDB=$1
LOGS=$2
TACTIC=$3
PORT=$4
LOGDIR=$5
PASS=`cat counter`

echo "mdb = $MDB  logs = $LOGS  tactic = $TACTIC  port = $PORT  pass = $PASS"
 
pushd .
cd ${LOGS}
    mkdir ${LOGDIR}
    mkdir ${LOGDIR}/pass_${PASS}
    rm -f ${LOGDIR}/pass_${PASS}/*
    mv log* ${LOGDIR}/pass_${PASS}/
cd ${LOGDIR}/pass_${PASS}
    loglist=""
    for logfile in `ls log*`
    do 
	if [ "$loglist" = "" ]
	then 
	    loglist=$logfile
	else
	    loglist=${loglist}`echo ","`${logfile}
	fi
    done
    echo $loglist
    rm -f flowmap
    ( ${BINDIR}/buildflows $MDB $loglist ) > flowmap
cd $MDB
    ${BINDIR}/assign -flowmap ${LOGS}/${LOGDIR}/pass_${PASS}/flowmap assigned.gml -o newassign.gml -cores machines.gml $TACTIC
    if [ ! -r ${MDB}/lastassign.gml ] 
    then
	echo "using initial assign as lastassign..."
	cp ${MDB}/assigned.gml ${MDB}/lastassign.gml
    fi
    if [ -r ${MDB}/newassign.gml ] 
    then
	rm -f changes
	( ${BINDIR}/assigndiff lastassign.gml newassign.gml ) > changes
	${BINDIR}/reassign . changes $PORT
	mv newassign.gml lastassign.gml
	mv changes ${LOGS}/${LOGDIR}/pass_${PASS}/
    else 
	echo "no assignment changes."
    fi
popd
