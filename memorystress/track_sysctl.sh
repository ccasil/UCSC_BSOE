if [ -z "$1" ]; then
	echo 'Please add an argument of the file to output logs to'
	exit 1
fi

is_memorystress_running=0
while sleep 0.05; do

	# Check if a memorystress process exists.
	ps aux | grep '[m]emorystress'
	if [ "$?" -eq "0" ]; then
		memorystress_exists=1
	else
		memorystress_exists=0
	fi

	# Memory stress was not running
	if [ "$is_memorystress_running" -eq "0" ]; then
		# but now it is.
		if [ "$memorystress_exists" -eq "1" ]; then
			is_memorystress_running=1
			echo '*~*~*~*~* memorystress started *~*~*~*~*' | tee -a $1
		fi

	# Memory stress was running
	elif [ "$is_memorystress_running" -eq "1" ]; then
		# but now it's not.
		if [ "$memorystress_exists" -eq "0" ]; then
			is_memorystress_running=0
			echo '*~*~*~*~* memorystress ended *~*~*~*~*' | tee -a $1
		fi
	fi
	echo '======' $(date) '======' | tee -a $1; sysctl -a | grep -f stats.txt | tee -a $1
done

