#!/usr/bin/ruby


# tests your peer downloading from our ref_peer
def test1
     
        peer1_pid = fork do
            exec("./ref_peer -p nodes.map -c A.chunks -f C.chunks -m 4 -i 1 -x 2 -d 0")
        end 
         
	parent_to_child_read, parent_to_child_write = IO.pipe
	peer2_pid = fork do
	    parent_to_child_write.close

	    $stdin.reopen(parent_to_child_read) or
			raise "Unable to redirect STDIN"

	    exec("./peer -p nodes.map -c B.chunks -f C.chunks -m 4 -i 2 -d 0")    
	end
	parent_to_child_read.close

	sleep 1.0
	## send message to standard in of peer
	write_to_peer = "GET test1.chunks test1.tar\n"
	parent_to_child_write.write(write_to_peer)
	parent_to_child_write.flush

	## wait for our ref_peer binary to stop
	pid = Process.waitpid(peer1_pid)
	return_code = $? >> 8;

	sleep 3.0
	if (return_code == 10) 

        	 diff_pid = fork do
            		exec("diff A.tar test1.tar")
       		 end 
		Process.waitpid(diff_pid)
		return_code = $? >> 8;
		if (return_code == 0) 
			puts "########### Test 1 Passed! ###########"
		else
			puts "Files A.tar and test1.tar do not match"
			puts "########### Test 1 Failed ###########"
		end
	else
		puts "ref_peer exited with failure"
		puts "try setting debug level to 63 for lots more output"
		puts "##########  Test 1 Failed #########"
	end

	Process.kill("SIGKILL", peer2_pid);
end 
	

# tests your peer uploading to our ref_peer
def test2
     
        peer1_pid = fork do
            exec("./peer -p nodes.map -c B.chunks -f C.chunks -m 4 -i 1  -d 0")
        end 
         
	parent_to_child_read, parent_to_child_write = IO.pipe
	peer2_pid = fork do
	    parent_to_child_write.close

	    $stdin.reopen(parent_to_child_read) or
			raise "Unable to redirect STDIN"

	    exec("./ref_peer -p nodes.map -c A.chunks -f C.chunks -m 4 -i 2 -x 2 -d 0")    
	end
	parent_to_child_read.close

	sleep 1.0
	## send message to standard in of peer
	write_to_peer = "GET test2.chunks test2.tar\n"
	parent_to_child_write.write(write_to_peer)
	parent_to_child_write.flush

	## wait for our ref_peer binary to stop
	pid = Process.waitpid(peer2_pid)
	return_code = $? >> 8;

	sleep 3.0
	if (return_code == 10) 

        	 diff_pid = fork do
            		exec("diff B.tar test2.tar")
       		 end 
		Process.waitpid(diff_pid)
		return_code = $? >> 8;
		if (return_code == 0) 
			puts "########### Test 2 Passed! ###########"
		else
			puts "Files B.tar and test2.tar do not match"
			puts "########### Test 2 Failed ###########"
		end
	else
		puts "ref_peer exited with failure"
		puts "try setting debug level to 63 for lots more output"
		puts "##########  Test 2 Failed #########"
	end

	Process.kill("SIGKILL", peer1_pid);
end 
# here's where we actually run the tests

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

puts "done with tests"

puts "killing spiffy"
Process.kill("SIGKILL", spiffy_pid)


