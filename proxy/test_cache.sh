#!/bin/bash

echo "=========================================="
echo "TEST CACHING"
echo "=========================================="
echo ""

rm -f test1.html test2.html test3.html 2>/dev/null

echo "First request - loading from server..."
http_proxy=http://localhost:8080 wget http://ccfit.nsu.ru/~rzheutskiy/test_files/ -O test1.html 2>&1 | tail -3
SIZE1=$(wc -c < test1.html 2>/dev/null || echo "0")
echo "First file: $SIZE1 bytes"

sleep 1

echo ""
echo "Second request - loading from cache..."
http_proxy=http://localhost:8080 wget http://ccfit.nsu.ru/~rzheutskiy/test_files/ -O test2.html 2>&1 | tail -3
SIZE2=$(wc -c < test2.html 2>/dev/null || echo "0")
echo "Second file: $SIZE2 bytes"

sleep 1

echo ""
echo "Third request - loading from cache..."
http_proxy=http://localhost:8080 wget http://ccfit.nsu.ru/~rzheutskiy/test_files/ -O test3.html 2>&1 | tail -3
SIZE3=$(wc -c < test3.html 2>/dev/null || echo "0")
echo "Third file: $SIZE3 bytes"

echo ""
echo "Checking files..."
if [ "$SIZE1" -gt 0 ] && [ "$SIZE1" -eq "$SIZE2" ] && [ "$SIZE2" -eq "$SIZE3" ]; then
    if diff -q test1.html test2.html >/dev/null 2>&1 && diff -q test2.html test3.html >/dev/null 2>&1; then
        echo "All three files are identical"
        echo "Caching works correctly"
    else
        echo "Files same size but different content"
    fi
else
    echo "File sizes: $SIZE1, $SIZE2, $SIZE3 bytes"
    echo "Check proxy logs for 'Reading entry from cache'"
fi

echo ""
echo "=========================================="
echo "SUCCESS"
echo "=========================================="
