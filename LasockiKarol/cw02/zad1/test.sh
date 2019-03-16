#!/bin/bash

for lines in 3333 6666; do
	for bytes in 1 4 512 1024 4096 8196; do
		echo "$lines records, $bytes bytes each"
		echo "---------------"
		echo ""
		./main generate tmp.txt "$lines" "$bytes"
		echo "---- LIB: ---- "
		./main copy tmp.txt tmp_lib.txt "$lines" "$bytes" lib
		./main sort tmp_lib.txt "$lines" "$bytes" lib
		echo "---- SYS ----"
                ./main copy tmp.txt tmp_sys.txt "$lines" "$bytes" sys
                ./main sort tmp_sys.txt "$lines" "$bytes" sys
		echo "____________________________"
	done

done

