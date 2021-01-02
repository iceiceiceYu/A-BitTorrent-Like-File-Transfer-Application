#!/usr/bin/perl
use strict;


srand(15441); #predictable seed
my $rand_drop_rate;
my %loss_lists;
my %dup_lists;

use IO::Handle;
my $DAT;

open($DAT, ">data_in_network.dat") || die("Cannot Open File: data_in_network.dat");
$DAT->autoflush(1);


# global hash for keeping track of how
# many packets each connection has in the network

my @packet_counts;
my @flow_starts;

my $QUEUEMAX = 10;
my $MAPFILE = "topo.map";
my $NODEFILE = "nodes.map";
my $PACKETSIZEMAX = 2048;

package NSQueue;

use IO::Socket::INET;

sub new ($) {

	my $self = {};

	my $class = shift;
	$self->{'routeRef'} = shift;
	my $ratebps = shift;
	$self->{'rate'} = int($ratebps / 8);
	$self->{'latency'} = shift;
	$self->{'max'} = shift || $QUEUEMAX;

	$self->{'size'} = 0;
	$self->{'verbose'} = 0;
	$self->{'dropped'} = 0;

	$self->{'queue'} = [];

	bless ($self, $class);
	return $self;
}

sub setVerbose($) {
	my $self = shift;
	$self->{'verbose'} = shift;
}

sub resetDropped() {
	my $self = shift;
	$self->{'dropped'} = 0;
}

sub getDropped() {
	my $self = shift;
	return $self->{'dropped'};
}

sub enQ ($$) {

	my $self = shift;
	my $pkt = shift;
	my $vtime = shift;
	my $queue = $self->{'queue'};

	if (@$queue >= $self->{'max'}) {
		# drop the packet
		if ($self->{'verbose'} > 1) { print "Dropping packet bound for ".$self->{'routeRef'}->{'id'}.": queue max is ".scalar(@$queue)."\n"; }
		$self->{'dropped'}++;

	} else {
		my $packSize = length($$pkt);
		my $txTime = $self->{'latency'} + ($packSize / $self->{'rate'}) + $vtime;
		if (@$queue > 0) {
#			$txTime += $queue->[0]->[1];
			$txTime += $self->{'size'} / $self->{'rate'};
		}
		if ($self->{'verbose'} > 2) { print "Enqueued packet bound for ".$self->{'routeRef'}->{'id'}.", $txTime\n"; }
		push(@$queue, [$pkt, $txTime]);
		$self->{'size'} += $packSize;
	}
}

package NSRouter;

my $INFINITY = 65535;	# must be bigger than # of routers
my $MICRO = 1000000;

use IO::Socket::INET;
use Time::HiRes qw(gettimeofday);

sub vecToString ($) {
	my $ipaddr = shift;
	return vec($ipaddr, 0, 8).".".vec($ipaddr, 1, 8).".".vec($ipaddr, 2, 8).".".vec($ipaddr, 3, 8);
}

sub new ($) {

	my $self = {};
	my $class = shift;

	$self->{'id'} = shift;
	$self->{'routes'} = [];
	$self->{'nodes'} = shift;
	my $nodeKeyString = shift;
	$self->{'sock'} = IO::Socket::INET->new(Proto => 'udp', PeerAddr => $nodeKeyString) || die ("Couldn't open router send socket on $nodeKeyString:  $!");

	$self->{'verbose'} = 0;
	$self->{'bytesSent'} = 0;

	bless($self, $class);
	return $self;
}

sub setVerbose($) {
	my $self = shift;
	$self->{'verbose'} = shift;
}

sub resetDropped() {
	my $self = shift;
	for my $queue (@{$self->{'routes'}}) {
		if (defined ($queue)) {
			$queue->resetDropped();
		}
	}
}

sub getDropped() {
	my $self = shift;
	my $dropped = 0;
	for my $queue (@{$self->{'routes'}}) {
		if (defined ($queue)) {
			$dropped += $queue->getDropped();
		}
	}
	return $dropped;
}

sub resetBytesSent() {
	my $self = shift;
	$self->{'bytesSent'} = 0;
}

sub getBytesSent() {
	my $self = shift;
	return $self->{'bytesSent'};
}

sub addRoute ($$$$) {

	my $self = shift;
	my $routeRef = shift;
	my $rate = shift;
	my $latency = shift;
	my $max = shift;

	$self->{'routes'}->[$routeRef->{'id'}] = new NSQueue($routeRef, $rate, $latency, $max);
	$self->{'routes'}->[$routeRef->{'id'}]->setVerbose($self->{'verbose'});
	if ($self->{'verbose'} > 0) { print "New queue added to ".$self->{'id'}.": $rate/$latency/max($max) to ".$routeRef->{'id'}."\n"; }
}

sub recv($$$) {

	my $self = shift;
	my $pkt = shift;
	my $vtime = shift;
	my $runRouters = shift;

	my ($srcID, $srcIP, $destIP, $srcPort, $destPort) = unpack("NNNnn", $$pkt);
	if (inet_ntoa(pack("N", $srcIP)) eq "0.0.0.0") { $srcIP = unpack("N", inet_aton("127.0.0.1")); }
	if (inet_ntoa(pack("N", $destIP)) eq "0.0.0.0") { $destIP = unpack("N", inet_aton("127.0.0.1")); }
	my $dest = $self->{'nodes'}->{pack("Nn", $destIP, $destPort)};

	if ($self->{'verbose'} > 2) { print "Router ".$self->{'id'}.": recv pkt at $vtime dest for ".inet_ntoa(pack("N", $destIP)).":$destPort (node '$dest')\n"; }
	unless (defined($dest)) {
		if ($self->{'verbose'} > 1) { print ("Received packet has no destination node, dropping...\n"); }
		return;
	}

	(my $packet_counts_key) = NSRouter::inet_ntoa(pack("N", $srcIP)).":$srcPort,"
				. NSRouter::inet_ntoa(pack("N", $destIP)).":$destPort";
	my $time_diff = int(($vtime * 1000.0) - $flow_starts[$packet_counts_key]);

	if ($dest == $self->{'id'}) {
		if ($self->{'verbose'} > 2) { print "Sending packet from router ".$self->{'id'}." to ".inet_ntoa(pack("N", $destIP)).":$destPort\n"; }
		$self->{'sock'}->send($$pkt);
		$self->{'bytesSent'} += (length($$pkt) - 16);

		## dw: we're delivering a packet OUT of the network
		$packet_counts[$packet_counts_key] = $packet_counts[$packet_counts_key] - 1;
		print $DAT "f$packet_counts_key\t$time_diff\t$packet_counts[$packet_counts_key]\n";

	} else {
		my $queue = $self->getQ($dest);
		if (defined ($queue)) {
			my $old_dropped = $queue->getDropped();
			$queue->enQ($pkt, $vtime);
			if($old_dropped != $queue->getDropped()) {
				# we just dropped a packet!
				print "queue loss!\n";
				$packet_counts[$packet_counts_key] = $packet_counts[$packet_counts_key] - 1;
				print $DAT "f$packet_counts_key\t$time_diff\t$packet_counts[$packet_counts_key]\n";
			}
			push (@$runRouters, $self);
#			print "Packet just enqueued in ".$self->{'id'}.", runRouters length is ".scalar(@$runRouters)."\n";
		} else {
			die ("Unable to locate queue for packet with dest $dest");
		}
	}
}

sub getQ($) {
	my $self = shift;
	my $dest = shift;

	return $self->{'routingTable'}->[$dest];
}

sub run($) {

	my $self = shift;
	my $runRouters = shift;

	my ($seconds, $microsec);

	my $wait = undef;

	for my $route (@{$self->{'routes'}}) {
		next unless defined($route);
		my $queue = $route->{'queue'};
		if ($self->{'verbose'} > 3) { print "Router ".$self->{'id'}."->".$route->{'routeRef'}->{'id'}." run: ".scalar(@$queue)." items, head is ".$queue->[0]->[1]."\n" if (@$queue > 0); }
#		print "Router ".$self->{'id'}."->".$route->{'routeRef'}->{'id'}." run: ".scalar(@$queue)." items, head is ".$queue->[0]->[1]."\n" if (@$queue > 0);

		# slightly imprecise subsecond counter
		($seconds, $microsec) = gettimeofday;
		while ((@$queue > 0) && ($queue->[0]->[1] <= ($seconds + ($microsec / $MICRO)))) {
			my ($pkt, $pTime) = @{shift(@$queue)};
			$route->{'size'} -= length($$pkt);
			($seconds, $microsec) = gettimeofday;
			$route->{'routeRef'}->recv($pkt, ($seconds + ($microsec / $MICRO)), $runRouters);
		}
		if (@$queue > 0) {
			my $packTime = $queue->[0]->[1] - ($seconds + ($microsec / $MICRO));
			if (defined($wait)) {
				$wait = $packTime if ($wait > $packTime);
			} else {
				$wait = $packTime;
			}	
		}
	}
	return $wait;
}

sub createTable($) {

	# creates routing table for all nodes using Dijkstra's
	# must be run before passing packets!

	my $self = shift;
	my $vertexes = shift;	# array of routers; index is router id

	$self->{'routingTable'} = [];

	# set up base state for all vertexes
	for my $vertex (@$vertexes) {
		next unless defined ($vertex);
		$vertex->{'done'} = 0;
		if ($vertex->{'id'} == $self->{'id'}) {
			$vertex->{'distance'} = 0;
		} else {
			$vertex->{'distance'} = $INFINITY;
		}
	}

	for my $vertex (@$vertexes) {
		next unless defined ($vertex);
		my $next;
		my $min = $INFINITY + 1;

		# figure out which vertex to visit next
		for my $vertexNext (@$vertexes) {
			next unless defined($vertexNext);
			if (!($vertexNext->{'done'}) && ($vertexNext->{'distance'} < $min)) {
				$next = $vertexNext;
				$min = $vertexNext->{'distance'};
			}
		}

		for my $edge (@{$next->{'routes'}}) {
			next unless defined ($edge);

# WEIGHT FUNCTION AS 1/{bitrate}
#			my $weight = $next->{'distance'} + (1/$edge->{'rate'});
# WEIGHT FUNCTION AS UNIT WEIGHT
			my $weight = $next->{'distance'} + 1;
			if ($edge->{'routeRef'}->{'distance'} > $weight) {
				$edge->{'routeRef'}->{'distance'} = $weight;
				$edge->{'routeRef'}->{'predecessor'} = $next;
			}
		}
		$next->{'done'}++;
	}
	for my $vertex (@$vertexes) {
		next unless defined ($vertex);
		next if ($vertex->{'id'} == $self->{'id'});
		my $startRoute = $vertex;
		while ($startRoute->{'predecessor'}->{'id'} != $self->{'id'}) {
			die ("route with no pred") unless (defined ($startRoute->{'predecessor'}));
			$startRoute = $startRoute->{'predecessor'};
		}

		if ($self->{'verbose'} > 3) { print "Router ".$vertex->{'id'}." accessible via ".$startRoute->{'id'}."\n"; }
		die("No queue for ".$vertex->{'id'}) unless defined ($self->{'routes'}->[$startRoute->{'id'}]);
		$self->{'routingTable'}->[$vertex->{'id'}] = $self->{'routes'}->[$startRoute->{'id'}];
	}
}

sub DESTROY() {

	my $self = shift;
	$self->{'sock'}->close() if (defined ($self->{'sock'}));
}

package main;

use FileHandle;
use Data::Dumper;
use Getopt::Std;
use IO::Socket;
use IO::Select;
use Time::HiRes qw(gettimeofday usleep);
use strict;

$| = 1;

my $profTime = getMicroTime();	# profiling time
my @routers;

$SIG{'HUP'} = sub {
	print "Last time: $profTime\n";
	my $curProfTime = getMicroTime();
	for my $router (@routers) {
		if (defined($router)) {
			print "Router ".$router->{'id'}.": ".($router->getBytesSent()/($curProfTime-$profTime))." bytes/sec\n";
			$router->resetBytesSent();
			for my $route (@{$router->{'routes'}}) {
				next unless defined($route);
				my $queue = $route->{'queue'};
				print "Queue ".$router->{'id'}."->".$route->{'routeRef'}->{'id'}.": ".$route->getDropped()." packets dropped, ".scalar(@$queue)." still in queue\n";
				$route->resetDropped();
			}
		}
	}

	print "Current time: $curProfTime\n";
	$profTime = $curProfTime;
};

sub getMicroTime() {
	my ($seconds, $microsec) = gettimeofday();
	my $MICRO = 1000000;
	return $seconds + ($microsec / $MICRO);
}

sub main() {

	my %opts;
	getopts(':l:d:p:m:n:i:v:', \%opts);
	my $port = $opts{'p'} || 30148;
	my $iaddr = $opts{'i'} || 'localhost';
	my $mapFile = $opts{'m'} || $MAPFILE;
	my $nodeFile = $opts{'n'} || $NODEFILE;
	my $verbose = $opts{'v'} || 0;
	my $lossfile = $opts{'l'} || 'drops.txt';
	my $dupfile = $opts{'d'} || 'dups.txt';

	my $fh;

	$fh = new FileHandle($nodeFile) || die ("Can't read $nodeFile");
	my %nodes;
	my %nodeKeys;

	for my $line (<$fh>) {
		next if ($line =~ m/^\#/);
		chomp($line);
		my ($node, $ip, $port) = split(/\s+/, $line);
		my $nodeKey = gethostbyname($ip).pack("n", $port);
		$nodes{$nodeKey} = $node;
		$nodeKeys{$node} = "$ip:$port";
	}
	$fh->close();

	$fh = new FileHandle($mapFile) || die ("Can't read $mapFile!"); 
#	my @routers = ();

	for my $line (<$fh>) {
		next if ($line =~ m/^\#/);
		chomp($line);
		my ($from, $to, $rate, $latency, $queuemax) = split(/\s+/, $line);
		unless (defined($routers[$to])) {
			if ($verbose > 0) { print "Creating new router $to\n"; }
			$routers[$to] = new NSRouter($to, \%nodes, $nodeKeys{$to});
			$routers[$to]->setVerbose($verbose);
		}
		unless (defined($routers[$from])) {
			if ($verbose > 0) { print "Creating new router $from\n"; }
			$routers[$from] = new NSRouter($from, \%nodes, $nodeKeys{$from});
			$routers[$from]->setVerbose($verbose);
		}

		$routers[$to]->addRoute($routers[$from], $rate, $latency, $queuemax);
		$routers[$from]->addRoute($routers[$to], $rate, $latency, $queuemax);
	}
	$fh->close();

	# added by DW to do specialized dropping	
	$fh = new FileHandle($lossfile) || die ("Can't read $lossfile");
	my $rand_drop_rate = <$fh>;

	print "drop rate = $rand_drop_rate \n";
	for my $line (<$fh>) {
		next if ($line =~ m/^\#/);
		chomp($line);
		my ($key, $droptimes_str) = split(/\s+/, $line);
		print "droptimes str = $droptimes_str \n";

		my @droptimes = split(/:/,$droptimes_str);		
		print "key = $key droptimes = @droptimes \n";
		$loss_lists{$key} = $droptimes_str;
	}
	$fh->close();

	# added by DW to do ack dup-ing
	$fh = new FileHandle($dupfile) || die ("Can't read $dupfile");

	for my $line (<$fh>) {
		next if ($line =~ m/^\#/);
		chomp($line);
		my ($key, $duptimes_str) = split(/\s+/, $line);
		my @duptimes = split(/:/,$duptimes_str);		
		print "key = $key duptimes = @duptimes \n";
		$dup_lists{$key} = $duptimes_str;
	}
	$fh->close();


	for my $router (@routers) {
		next unless defined($router);
		$router->createTable(\@routers);
	}

	# ok, topology is set up, start listening for packets

	my $sleepSeconds = undef;

	my $sock = IO::Socket::INET->new(Proto => 'udp', LocalAddr => $iaddr, LocalPort => $port) || die ("Couldn't open socket:  $!");
	my $sel = new IO::Select($sock);

	my ($seconds, $microsec);
	my $systimesec;

	my @runRouters;

	print "Listening on $port...\n";
	LOOP: while (1) {
		$systimesec = sprintf("%d.%06d", $seconds, $microsec);
		if ($verbose > 3) { print "Time is $systimesec.\n"; }
		if (my ($selSock) = $sel->can_read($sleepSeconds)) {
#		if (my ($selSock) = $sel->can_read(0)) {
			($seconds, $microsec) = gettimeofday();
			my $pkt;
			$selSock->recv($pkt, $PACKETSIZEMAX);
			if ($verbose > 3) { print "Got a packet\n"; }
			my($port, $ipaddr) = sockaddr_in($selSock->peername);

			my ($srcID, $srcIP, $destIP, $srcPort, $destPort, $j1, $j2, $seq, $ack) = unpack("NNNnnNNNN", $pkt);
			my $type = $j1 & 31;
			if (inet_ntoa(pack("N", $srcIP)) eq "0.0.0.0") { $srcIP = unpack("N", inet_aton("127.0.0.1")); }
			if (inet_ntoa(pack("N", $destIP)) eq "0.0.0.0") { $destIP = unpack("N", inet_aton("127.0.0.1")); }
			if ($verbose > 3) {
				print "spiffy_header src: ".NSRouter::inet_ntoa(pack("N", $srcIP)).":$srcPort\n";
				print "spiffy_header dst: ".NSRouter::inet_ntoa(pack("N", $destIP)).":$destPort\n";
				print "packet contents: ".Dumper(substr($pkt, 16));
			}

			my $millis = ($seconds + ($microsec/ $MICRO)) * 1000.0;
			(my $key) = NSRouter::inet_ntoa(pack("N", $srcIP)).":$srcPort,"
							. NSRouter::inet_ntoa(pack("N", $destIP)).":$destPort";
			
			if (exists $packet_counts[$key]) {
				$packet_counts[$key] = $packet_counts[$key] + 1;
			} else {
				print "starting flow $key at $millis\n";
				$packet_counts[$key] = 1;
				$flow_starts[$key] = $millis;
			}
			my $time_diff = int($millis - $flow_starts[$key]);
			print $DAT "f$key\t$time_diff\t$packet_counts[$key]\t$seq\t$ack\t$type\n";

			my $num_packet_copies = 1; # default, no packet duplication
			# only drop or duplcate packets if they are data or acks
			if($type == 3 || $type == 4) {
				my $rand_test = rand(1);
				my $test1 = $rand_drop_rate > $rand_test;
				my $test2 = 0; # false by default
				if ( (exists $loss_lists{$key}) && ( length($loss_lists{$key}) > 0)){
					my @droptimes = split(/:/,$loss_lists{$key});		
					my $next_loss = $droptimes[0];
#					print "next loss at $next_loss (cur_diff = $time_diff)\n";
					if($next_loss < $seq) {
			#		if($next_loss < $time_diff) {
						print "Dropping (seq = $seq ack = $ack) at $time_diff\n";
						$test2 = 1;
						shift @droptimes;
						$loss_lists{$key} = join(':', @droptimes);
					}
				}

				if($test1 || $test2) {
					my $str_len = length($pkt);
					print "Dropping packet seq = $seq, ack = $ack len = $str_len"
						." (from,to) = ($key) at time = $time_diff \n";
					$packet_counts[$key] = $packet_counts[$key] - 1;
					print $DAT "f$key\t$time_diff\t$packet_counts[$key]\n";
					# this is just a hack to let us see drops in the graph
					print $DAT "f$key\t$time_diff\t-1\t$seq\t$ack\n";
					next LOOP;	
				}
			
				if ( (exists $dup_lists{$key}) && ( length($dup_lists{$key}) > 0)){
					my @duptimes = split(/:/,$dup_lists{$key});		
					my $next_dup = $duptimes[0];
					if($next_dup < $time_diff) {
						$num_packet_copies = 4; # enough to trigger a dup ack
						print "Transmit (seq = $seq, ack = $ack) $num_packet_copies times!!! at $time_diff\n";
						shift @duptimes;
						$dup_lists{$key} = join(':', @duptimes);
						$packet_counts[$key] = $packet_counts[$key] + 3;
						print $DAT "f$key\t$time_diff\t$packet_counts[$key]\n";
					}
				}
			} # end if($type == 3 || $type == 4) 

			my $nodeID = unpack("N", $pkt);
			if ($verbose > 2) { print "spiffy_header nodeID: $nodeID\n"; }
			if (defined($routers[$nodeID])) {
				if ($verbose > 3) { print "Inserting packet at node $nodeID\n"; }
				($seconds, $microsec) = gettimeofday();
				my $i = 0;
				while($i < $num_packet_copies) {
					$routers[$nodeID]->recv(\$pkt, ($seconds + ($microsec / $MICRO)), \@runRouters);
					$i = $i + 1;
				}
			}
		}
		$sleepSeconds = undef;
#		$sleepSeconds = 5;
		my $wait;

		for my $router (@routers) {
			if (defined ($router)) {
				push @runRouters, $router;
			}
		}

#		for my $router (@routers) {
		while (my $router = shift(@runRouters)) {
			$wait = $router->run(\@runRouters);
			if ($verbose > 3) { print "router ".$router->{'id'}." waits $wait\n"; }
			if (defined($sleepSeconds)) {
				$sleepSeconds = $wait if (defined ($wait) && ($wait < $sleepSeconds));
			} else {
				$sleepSeconds = $wait;
			}
		}
		if ($verbose > 3) { print "Next socket timeout is ".($sleepSeconds)." seconds...\n"; }
	}

}


main();

