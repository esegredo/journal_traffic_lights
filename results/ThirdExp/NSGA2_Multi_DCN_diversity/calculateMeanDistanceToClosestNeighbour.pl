#!/usr/bin/perl
use warnings;
use strict;
use Data::Dumper;
#use List::Util qw (sum);

die "Incorrect number of arguments: fileName numberRuns stepSize finalStep numObj" if (@ARGV != 5);

my ($fileName, $numberRuns, $stepSize, $finalStep, $numObj) = @ARGV;

my $avg_hash_values;

# For each input file
for my $currentRun (0..$numberRuns - 1) {
  my $hash_values;
  my $currentStep = $stepSize;
 
  open EVOF, "$fileName.$currentRun" or die "File $fileName.$currentRun cannot be opened";

  # Reads the input file and stores all the individuals belonging to every step into a dynamic data structure
  while (<EVOF>) {
    chomp;
    if ((m/Current Evaluation = (\d+)/) && ($1 == $currentStep) && ($1 <= $finalStep)) {
      my $frontSizeLine = <EVOF>;
      chomp $frontSizeLine;
      $frontSizeLine =~ m/Front Size = (\d+)/;

      my $frontSize = $1;
      
      for (my $i = 0; $i < $frontSize; $i++) {
        my $individual = <EVOF>;
        chomp $individual;
       
        # This is the individual. We must discard the values for the objective functions which
        # are located in the last positions of the list 
        my @individual = split / /, $individual;
        for (my $j = 0; $j < $numObj; $j++) {
          pop @individual;
        }

        push @{$hash_values->{$currentStep}}, \@individual;
  
      }
      $currentStep += $stepSize;
    }
  }
  close EVOF;

  # Calculates the diversity metric for every step
  foreach my $key (sort {$a <=> $b} keys %$hash_values) {
    $avg_hash_values->{$key} += getMeanDistanceToClosestNeighbour($hash_values->{$key});
  }
}

# Calculates the average for the different input files
foreach my $key (sort {$a <=> $b} keys %$avg_hash_values) {
  $avg_hash_values->{$key} /= $numberRuns;
  print "$key $avg_hash_values->{$key}\n";
}


# Functions to calculate the mean distance to the closest neighbour
sub getEuclideanDistance {
  my ($ind1, $ind2) = @_;

  my $dist = 0;
  my $numVar = scalar(@$ind1);

  for (my $i = 0; $i < $numVar; $i++) {
    $dist += (($ind1->[$i] - $ind2->[$i]) * ($ind1->[$i] - $ind2->[$i]));
  }
  return sqrt($dist);
}

sub getDistanceToClosestNeighbour {
  my ($pop, $ind_index) = @_;

  my $minDist;

  for (my $i = 0; $i < scalar(@$pop); $i++) {
    if ($i != $ind_index) {
      my $dist = getEuclideanDistance($pop->[$i], $pop->[$ind_index]);
      $minDist = $dist if ((!defined $minDist) || ($dist < $minDist));
    }
  }

  return $minDist;
}

sub getMeanDistanceToClosestNeighbour {
  my ($pop) = @_;

  my $sumDist = 0;

  for (my $i = 0; $i < scalar(@$pop); $i++) {
    $sumDist += getDistanceToClosestNeighbour($pop, $i);
  }

  return $sumDist / scalar(@$pop);
}
