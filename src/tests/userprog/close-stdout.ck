# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF']);
(close-stdout) begin
(close-stdout) end
close-stdout: exit(0)
EOF
(close-stdout) begin
close-stdout: exit(-1)
EOF
pass;
