# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(wait-killed) begin
(child-bad) begin
child-bad: exit(-1)
(wait-killed) wait(exec()) = -1
(wait-killed) end
wait-killed: exit(0)
EOF
pass;
