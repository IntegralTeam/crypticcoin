#!/usr/bin/env bash



./src/crypticcoin-cli --datadir=/home/egor-l/.mn0 createraw_mn_announce '[]' '{
"name" : "mn0",
"ownerAuthAddress" : "c1bkjTZ9MAEdb6rTy3CWJRhMz3mUAkMuyji",
"operatorAuthAddress" : "c1QZqDerDLbqkAKairjkCy43gdQhieQvnZJ",
"ownerRewardAddress" : "c1gfErrF3NYkeGNKTyLK8qk4845vFBUqqtV",
"collateralAddress" : "c1kjdmh5R18v6G26w5PVMdWX7XK7MgR2sUS"
}'

./src/crypticcoin-cli --datadir=/home/egor-l/.mn1 createraw_mn_announce '[]' '{
"name" : "mn1",
"ownerAuthAddress" : "c1TVyXhnKCfs2qJ2NSTGE78vNjSBZKULXpL",
"operatorAuthAddress" : "c1kZapE5yaJKwWYBx68YNPL87uwZK3moyaW",
"ownerRewardAddress" : "c1odV2zFB5UP8FUhx5CBpy2yCadQsPPcwDz",
"collateralAddress" : "c1XdHuz6tD3ZLyd6dyqi676qz4e3wg5L3eq"
}'

./src/crypticcoin-cli --datadir=/home/egor-l/.mn2 createraw_mn_announce '[]' '{
"name" : "mn2",
"ownerAuthAddress" : "c1TbFrqcgw68wM3fHrmmQ4BSNj97SKJqezE",
"operatorAuthAddress" : "c1aakrTTLZDGugRfVZ6AAjds9fiXaKap2R2",
"ownerRewardAddress" : "c1RdZCcE7ASvHJUQQ21NzAJm33f6oAZ9Rhz",
"collateralAddress" : "c1e1E73WsMv6dxQZxF9DmQAVkjKQrNFNbK7"
}'