<?php
/**
 * Created by PhpStorm.
 * Date: 11/13/15
 * Time: 14:20
 */
include '../build/bin/php_include.php';
header('Content-Type: application/json');
if (file_exists('./login')) {
    echo "{'success': 'true'}";
    unlink('./login');
    touch('../build/bin/states/'.$username.'_refuse');
} else {
    echo "{'success': 'false'}";
}
