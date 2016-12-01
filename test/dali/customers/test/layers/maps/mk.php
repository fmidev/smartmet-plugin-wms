#!/usr/bin/php -q
<?php

$rows = file("tables");
foreach($rows as $row)
{
    list($schema,$table) = explode(" ",trim($row));

    $name = str_replace("_eureffin","",$table);
    $name = str_replace("_wgs84","",$name);
    $name = str_replace("_ykj","",$name);
    $name = str_replace("_fmi20","",$name);

    $str =<<<EOT
{
	"schema": "$schema",
	"table": "$table"
}

EOT;
    $filename = "${schema}/${name}.json";
    echo "$filename:\n$str";
    file_put_contents($filename,$str);
}
