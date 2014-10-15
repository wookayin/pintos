# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF']);
(close-stdin) begin
(close-stdin) end
close-stdin: exit(0)
EOF
(close-stdin) begin
close-stdin: exit(-1)
EOF
pass;
