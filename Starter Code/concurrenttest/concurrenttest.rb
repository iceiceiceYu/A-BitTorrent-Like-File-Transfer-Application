#!/usr/bin/ruby


# tests your peer downloading from our ref_peer
def test1
     
        peer1_pid = fork do
            exec("./ref_peer -p nodes.map -c A.chunks -f ./C.chunks -m 4 -i 1 -d 3 -x 1")
        end 
        
        peer2_pid = fork do
            exec("./ref_peer -p nodes.map -c A.chunks -f ./C.chunks -m 4 -i 2 -d 3 -x 1")
        end 
	sleep 1.0 

	parent_to_child_read, parent_to_child_write = IO.pipe
	peer3_pid = fork do
	    parent_to_child_write.close

	    $stdin.reopen(parent_to_child_read) or
			raise "Unable to redirect STDIN"

	    exec("./peer -p nodes.map -c B.chunks -f ./C.chunks -m 4 -i 3 -d 3")    
	end
	parent_to_child_read.close

	sleep 1.0
	## send message to standard in of peer
	write_to_peer = "GET test1.chunks test1.tar\n"
	parent_to_child_write.write(write_to_peer)
	parent_to_child_write.flush

	## wait for our ref_peer binaries to stop
	pid = Process.waitpid(peer1_pid)
	pid = Process.waitpid(peer2_pid)
	return_code = $? >> 8;

	sleep 3.0 
	if (return_code == 10) 

        	 diff_pid = fork do
            		exec("diff ./A.tar test1.tar")
       		 end 
		Process.waitpid(diff_pid)
		return_code = $? >> 8;
		if (return_code == 0) 
			puts "########### Test 1 Data is Correct ###########"
		else
			puts "Files A.tar and test1.tar do not match"
			puts "########### Test 1 Failed ###########"
		end
	else
		puts "ref_peer exited with failure"
		puts "try setting debug level to 63 for lots more output"
		puts "##########  Test 1 Failed #########"
	end

	system("rm test1.tar");
	Process.kill("SIGKILL", peer3_pid);
end 
	

# here's where we actually run the tests



## CHANGE ME!  
spiffy_port = 15441


ENV['SPIFFY_ROUTER'] = "127.0.0.1:#{spiffy_port}"   

system("rm -rf test1.tar")
system("rm -rf data_in_network.dat")
system("rm -rf problem2-peer.txt")

puts "starting SPIFFY on port #{spiffy_port}"

spiffy_pid = fork do
	exec("perl ./hupsim_adv.pl -m topo.map -n nodes.map -p #{spiffy_port} -v 0 -l test1_drop.txt -d test1_dup.txt")
end

sleep 2.0

puts "starting test"

test1

puts "killing spiffy after test1"
Process.kill("SIGKILL", spiffy_pid)

puts "done with test"

system("rm -rf data_in_network.dat")
