#!/bin/perl -w
use strict;

# first make sure that we were given two file names
my $file1 = shift();
my $file2 = shift();
if (!$file1 || !$file2) { die "invalid usage"; }

# all the benchmark names, thread names, and validation types used
my @p_bm;
my @p_thr;
my @p_val;

# all the data from each of the files
my %dat1;
my %dat2;

# temp vars
my $bm = "";
my $thr = 0;
my $val = "";
my $txsec = 0;

open (F1, $file1);
while (<F1>) {
    # does this line give the benchmark name and the thread count
    if    (/(.*), d=.*elements, (.*) thread/)  {$bm = $1; $thr = int($2);}
    # does this line give us the validation strategy
    elsif (/^(.*)-(.*)$/)                      {$val = $1."-".$2;}
    # does this line give us the tx/sec value?
    elsif (/(.*) txns per second/) {
        $txsec = $1;

        # save the record to the hash
        $dat1{"$bm$val$thr"} = $txsec;
        
        # remember the components of the hash key
        push (@p_bm, $bm);
        push (@p_val, $val);
        push (@p_thr, $thr);
    }
}
close (F1);

open (F2, $file2);
while (<F2>) {
    if    (/(.*), d=.*elements, (.*) thread/)  {$bm = $1; $thr = int($2);}
    elsif (/^(.*)-(.*)$/)                      {$val = $1."-".$2;}
    elsif (/^(.*) txns per second/) {
        $txsec = $1;
        $dat2{"$bm$val$thr"} = $txsec;

        push (@p_bm, $bm);
        push (@p_val, $val);
        push (@p_thr, $thr);
    }
}
close (F2);

# remove the duplicates from the index arrays
my %h = map {$_ => 1} @p_bm;
@p_bm = sort keys %h;
%h = map {$_ => 1} @p_val;
@p_val = sort keys %h;
%h = map {$_ => 1} @p_thr;
@p_thr = sort { $a <=> $b } keys %h;

# now print the tables, with an extra column for the % improvement of col2
foreach my $bm (@p_bm) {
    foreach my $v (@p_val) {
        print "$bm $v\n";
        print "$file1 -vs- $file2\n";
        print "------------------------------\n";
        foreach my $t (@p_thr) {
            my $a = $dat1{"$bm$v$t"} | 1;
            my $b = $dat2{"$bm$v$t"} | 1;
            print "$t\t";
            print $a . "\t";
            print $b . "\t";
            my $c = (($b-$a)/$b);
            my $d = '';
            $d = '+' if ($c>=0);
            $c = int(10000.0*$c)/100;
            print $d.$c . "%\n";
        }
    }
    print "\n";
}
