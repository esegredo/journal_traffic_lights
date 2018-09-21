#!/usr/bin/perl
use warnings;
use strict;
use Data::Dumper;
use POSIX;
#use List::Util qw (sum);

die "Incorrect number of arguments: fileName numberRuns stepSize finalStep popSize numObj" if (@ARGV != 6);

my ($fileName, $numberRuns, $stepSize, $finalStep, $popSize, $numObj) = @ARGV;

my $avg_hash_values;

# For each input file
for my $currentRun (0..$numberRuns - 1) {
  my $hash_values;
  my $currentStep = $stepSize;
 
  open EVOF, "$fileName.$currentRun" or die "File $fileName.$currentRun cannot be opened";

  # Reads the input file and stores all the individuals belonging to every step into a dynamic data structure
  while (<EVOF>) {
    chomp;
    if ((m/time: (\d+)/) && (floor($1 / $stepSize) * $stepSize <= $finalStep) && ($currentStep <= $finalStep)) {
      my $actualTime = floor($1 / $stepSize) * $stepSize;
      my $numberUpdates = ($actualTime - $currentStep) / $stepSize;

      for (my $i = 0; $i < $popSize; $i++) {
        my $individual = <EVOF>;
        chomp $individual;
       
        # This is the individual. We must discard the values for the objective functions which
        # are located in the last positions of the list 
        my @individual = split / /, $individual;
        for (my $j = 0; $j < $numObj; $j++) {
          pop @individual;
        }

        my $midStep = $currentStep;
        for (my $j = 0; $j <= $numberUpdates; $j++) {
          push @{$hash_values->{$midStep}}, \@individual;
          $midStep += $stepSize;
        }
      }
      $currentStep += (($numberUpdates + 1) * $stepSize);
    }
  }
  close EVOF;

  # Calculates the diversity metric for every step
  foreach my $key (sort {$a <=> $b} keys %$hash_values) {
    $avg_hash_values->{$key} += getMeanPairwiseDistanceAllIndividuals($hash_values->{$key});
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

sub getMeanPairwiseDistanceAllIndividuals {
  my ($pop) = @_;

  my $sumDist = 0;

  for (my $i = 0; $i < scalar(@$pop); $i++) {
    for (my $j = $i + 1; $j < scalar(@$pop); $j++) {
      $sumDist += getEuclideanDistance($pop->[$i], $pop->[$j]);
    }
  }

  return $sumDist / (scalar(@$pop) * (scalar(@$pop) - 1) / 2);
}
