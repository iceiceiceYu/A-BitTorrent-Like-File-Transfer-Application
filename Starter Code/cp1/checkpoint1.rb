#!/usr/bin/ruby


# tests ability of your peer to send a correct WHOHAS request
def test1
     
        cp_pid = fork do
            exec("./cp1_tester -p nodes.map -c A.chunks -f C.chunks -m 4 -i 1 -t 1 -d 15")
        end
         
	parent_to_child_read, parent_to_child_write = IO.pipe
	peer_pid = fork do
	    parent_to_child_write.close

	    $stdin.reopen(parent_to_child_read) or
			raise "Unable to redirect STDIN"

	    exec("./peer -p nodes.map -c B.chunks -f C.chunks -m 4 -i 2 -d 15")
	end
	parent_to_child_read.close

	sleep 1.0
	## send message to standard in of peer
	write_to_peer = "GET A.chunks silly.tar\n"
	parent_to_child_write.write(write_to_peer)
	parent_to_child_write.flush

	## wait for checkpoint1 binary to stop
	pid = Process.waitpid(cp_pid)
	return_code = $? >> 8;

	if(pid == cp_pid)
		if (return_code == 10) 
			puts "########### Test 1 Passed! ###########\n"
		else
			puts "##########  Test 1 Failed #########\n"
		end
	else
		puts "Error running test1.  Other process exited before checkpoint process"
		puts "##########  Test 2 Failed #########\n"
	end

	Process.kill("SIGKILL", peer_pid);
end 
	

# tests ability of peer to respond to a WHOHAS with an IHAVE they have all files requested
def test2
	
	peer_pid = fork do

	    exec("./peer -p nodes.map -c A.chunks -f C.chunks -m 4 -i 2 -d 15")    
	end
	
	sleep 1.0

        cp_pid = fork do
            exec("./cp1_tester -p nodes.map -c B.chunks -f C.chunks -m 4 -i 1 -t 2 -d 15")
        end 

	## wait for checkpoint1 binary to stop
	pid = Process.waitpid(cp_pid)
	return_code = $? >> 8;
	if(pid == cp_pid) 
		if (return_code == 10)
			puts "########### Test 2 Passed! ###########\n"
		else
			puts "##########  Test 2 Failed #########\n"
		end
	else
		puts "Error running test2.  Other process " + pid.to_s + 
		" exited before checkpoint process with code " + return_code.to_s
		puts "##########  Test 2 Failed #########\n"
		Process.kill("SIGKILL", cp_pid)
	end

	Process.kill("SIGKILL", peer_pid);
end 


# Makes sure that you don't send back an IHAVE, if you don't have any of the
# chunks requested in a WHOHAS.  In this case, both peers are configured
# to have B.chunks, and the checkpoint peer will send a request for A.chunks
def test3
	
	peer_pid = fork do

	    exec("./peer -p nodes.map -c A.chunks -f C.chunks -m 4 -i 2 -d 15")    
	end
	
	sleep 1.0

        cp_pid = fork do
            exec("./cp1_tester -p nodes.map -c A.chunks -f C.chunks -m 4 -i 1 -t 3 -d 15")

        end 

	## wait for checkpoint1 binary to stop
	pid = Process.waitpid(cp_pid)
	return_code = $? >> 8;
	if(pid == cp_pid) 
		if (return_code == 10)
			puts "########### Test 3 Passed! ###########\n"
		else
			puts "##########  Test 3 Failed #########\n"
		end
	else
		puts "Error running test3.  Other process " + pid.to_s + 
		" exited before checkpoint process with code " + return_code.to_s
		puts "##########  Test 3 Failed #########\n"
		Process.kill("SIGKILL", cp_pid)
	end

	Process.kill("SIGKILL", peer_pid);
end 

# here's where we actually run the tests


# comment out these 3 lines, once you've read the README
# puts "Make sure you carefully read the README!"
# puts "Then comment these lines out.  Exiting..."
# exit 0

if (!File.exists?("peer"))
	puts "Error: need to have binary named 'peer' in this directory"
	exit 1
end 


## CHANGE ME!  
spiffy_port = 15441


ENV['SPIFFY_ROUTER'] = "127.0.0.1:#{spiffy_port}"   

puts "starting SPIFFY on port #{spiffy_port}"

spiffy_pid = fork do
	exec("perl ./hupsim.pl -m topo.map -n nodes.map -p #{spiffy_port} -v 0")
end

puts "starting tests"

test1

sleep 3.0

test2

sleep 3.0

test3

puts "done with tests"

puts "killing spiffy"
Process.kill("SIGKILL", spiffy_pid)


