# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF']);
(multi-child-fd) begin
(multi-child-fd) open "sample.txt"
(child-close) begin
(child-close) end
child-close: exit(0)
(multi-child-fd) wait(exec()) = 0
(multi-child-fd) verified contents of "sample.txt"
(multi-child-fd) end
multi-child-fd: exit(0)
EOF
(multi-child-fd) begin
(multi-child-fd) open "sample.txt"
(child-close) begin
child-close: exit(-1)
(multi-child-fd) wait(exec()) = -1
(multi-child-fd) verified contents of "sample.txt"
(multi-child-fd) end
multi-child-fd: exit(0)
EOF
pass;
