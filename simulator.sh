for plr in {0.01,0.1,0.2};
do
	for dr in {0.1,0.2,0.3,0.5,0.7,1};
	do
		for sdr in {2,3,4};
		do
			OLDIFS=$IFS; 
			IFS=','; 
			for i in 4,1000 20,5000 40,10000 200,50000 200,60000; 
			do 
				set -- $i;  
				for ubp in "true", "false";
				do
					./waf --run "scratch/test-simple-epc \
					--useBackup="$ubp" --streamDataRate="$sdr"Mbps \
					--playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
					--simTime="$1" --numberOfPackets="$2" \
					--retran_time_out=0.25"
				done
			done; IFS=$OLDIFS
		# # 1000 pkts
		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=false --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=4 --numberOfPackets=1000 \
		# --retran_time_out=0.25"

		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=true --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=4 --numberOfPackets=1000 \
		# --retran_time_out=0.25"

		# # 5000 pkts
		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=false --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=20 --numberOfPackets=5000 \
		# --retran_time_out=0.25"

		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=true --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=20 --numberOfPackets=5000 \
		# --retran_time_out=0.25"

		# # 10000 pkts
		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=false --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=40 --numberOfPackets=10000 \
		# --retran_time_out=0.25"

		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=true --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=40 --numberOfPackets=10000 \
		# --retran_time_out=0.25"

		# # 50000 pkts max
		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=false --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=200 --numberOfPackets=50000 \
		# --retran_time_out=0.25"

		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=true --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=200 --numberOfPackets=50000 \
		# --retran_time_out=0.25"

		# # 60000 pkts
		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=false --streamDataRate="$sdr"Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=200 --numberOfPackets=60000 \
		# --retran_time_out=0.25"

		# ./waf --run "scratch/test-simple-epc \
		# --useBackup=true --streamDataRate=3Mbps \
		# --playDataRate=3Mbps --plr="$plr" --dupRate="$dr" \
		# --simTime=200 --numberOfPackets=60000 \
		# --retran_time_out=0.25"
		done
	done
done

