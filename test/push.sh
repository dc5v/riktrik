#!/bin/bash

echo '{"query":"push", "data":[1], "tags":["tag1"]}' | nc localhost 8832
echo '{"query":"evaluate", "tags": ["tag1"]}'  | nc localhost 8832
