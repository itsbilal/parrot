#! parrot
# Copyright (C) 2006-2008, The Perl Foundation.
# $Id$

=head1 NAME

t/pmc/enumerate.t - test Enumerate PMC

=head1 SYNOPSIS

    % prove t/pmc/enumerate.t

=head1 DESCRIPTION

Tests the Enumerate PMC.

=cut

.sub main :main
    .include 'test_more.pir'

    plan(1)

    $P1 = new ['Array']
    $P2 = new ['Enumerate'], $P1

    ok(1, "Instantiated 'Enumerate'")
.end

# Local Variables:
#   mode: pir
#   fill-column: 100
# End:
# vim: expandtab shiftwidth=4 ft=pir:
