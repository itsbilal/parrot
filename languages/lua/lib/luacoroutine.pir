# Copyright: 2005 The Perl Foundation.  All Rights Reserved.
# $Id$

=head1 NAME

lib/luacoroutine.pir - Lua Coroutine Library

=head1 DESCRIPTION

The operations related to coroutines comprise a sub-library of the basic
library and come inside the table C<coroutine>.

See "Lua 5.0 Reference Manual", section 5.2 "Coroutine Manipulation".

=head2 Functions

=over 4

=cut

.namespace [ "Lua" ]
.HLL "Lua", "lua_group"


.sub init :load, :anon

    load_bytecode "languages/lua/lib/luapir.pbc"
    load_bytecode "languages/lua/lib/luabasic.pbc"

#    print "init Lua Coroutine\n"

    .local pmc _lua__G
    _lua__G = global "_G"
    $P1 = new .LuaString

    .local pmc _coroutine
    _coroutine = new .LuaTable
    $P1 = "coroutine"
    _lua__G[$P1] = _coroutine

    .const .Sub _coroutine_create = "_coroutine_create"
    $P0 = _coroutine_create
    $P1 = "create"
    _coroutine[$P1] = $P0

    .const .Sub _coroutine_resume = "_coroutine_resume"
    $P0 = _coroutine_resume
    $P1 = "resume"
    _coroutine[$P1] = $P0

    .const .Sub _coroutine_status = "_coroutine_status"
    $P0 = _coroutine_status
    $P1 = "status"
    _coroutine[$P1] = $P0

    .const .Sub _coroutine_wrap = "_coroutine_wrap"
    $P0 = _coroutine_wrap
    $P1 = "wrap"
    _coroutine[$P1] = $P0

    .const .Sub _coroutine_yield = "_coroutine_yield"
    $P0 = _coroutine_yield
    $P1 = "yield"
    _coroutine[$P1] = $P0

.end

=item C<coroutine.create (f)>

Creates a new coroutine, with body C<f>. C<f> must be a Lua function.
Returns this new coroutine, an object with type C<"thread">.

NOT YET IMPLEMENTED.

=cut

.sub _coroutine_create :anon
    not_implemented()
.end

=item C<coroutine.resume (co, val1, ...)>

Starts or continues the execution of coroutine C<co>. The first time you
resume a coroutine, it starts running its body. The arguments C<val1, ...>
go as the arguments to the body function. If the coroutine has yielded,
C<resume> restarts it; the arguments C<val1, ...> go as the results from the
yield.

If the coroutine runs without any errors, C<resume> returns B<true> plus any
values passed to yield (if the coroutine yields) or any values returned by
the body function (if the coroutine terminates). If there is any error,
C<resume> returns B<false> plus the error message.

NOT YET IMPLEMENTED.

=cut

.sub _coroutine_resume :anon
    not_implemented()
.end

=item C<coroutine.status (co)>

Returns the status of coroutine C<co>, as a string: C<"running">, if the
coroutine is running (that is, it called C<status>); C<"suspended">, if the
coroutine is suspended in a call to yield, or if it has not started running
yet; and C<"dead"> if the coroutine has finished its body function, or if it
has stopped with an error.

NOT YET IMPLEMENTED.

=cut

.sub _coroutine_status :anon
    not_implemented()
.end

=item C<coroutine.wrap (f)>

Creates a new coroutine, with body C<f>. C<f> must be a Lua function.
Returns a function that resumes the coroutine each time it is called. Any
arguments passed to the function behave as the extra arguments to C<resume>.
Returns the same values returned by C<resume>, except the first boolean. In
case of error, propagates the error.

NOT YET IMPLEMENTED.

=cut

.sub _coroutine_wrap :anon
    not_implemented()
.end

=item C<coroutine.yield (val1, ...)>

Suspends the execution of the calling coroutine. The coroutine cannot be
running neither a C function, nor a metamethod, nor an iterator.
Any arguments to C<yield> go as extra results to C<resume>.

NOT YET IMPLEMENTED.

=cut

.sub _coroutine_yield :anon
    not_implemented()
.end

=back

=head1 AUTHORS


=cut

