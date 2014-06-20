# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(exec-once) begin
(child-simple) run
child-simple: exit(81)
(exec-once) end
exec-once: exit(0)
EOF
pass;
