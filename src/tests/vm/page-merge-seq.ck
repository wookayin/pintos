# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(page-merge-seq) begin
(page-merge-seq) init
(page-merge-seq) sort chunk 0
(page-merge-seq) sort chunk 1
(page-merge-seq) sort chunk 2
(page-merge-seq) sort chunk 3
(page-merge-seq) sort chunk 4
(page-merge-seq) sort chunk 5
(page-merge-seq) sort chunk 6
(page-merge-seq) sort chunk 7
(page-merge-seq) sort chunk 8
(page-merge-seq) sort chunk 9
(page-merge-seq) sort chunk 10
(page-merge-seq) sort chunk 11
(page-merge-seq) sort chunk 12
(page-merge-seq) sort chunk 13
(page-merge-seq) sort chunk 14
(page-merge-seq) sort chunk 15
(page-merge-seq) merge
(page-merge-seq) verify
(page-merge-seq) success, buf_idx=1,032,192
(page-merge-seq) end
EOF
pass;
