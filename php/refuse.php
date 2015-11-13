<?php
/**
 * Created by PhpStorm.
 * Date: 11/13/15
 * Time: 14:20
 */
header('Content-Type: application/json');
if (file_exists('./login')) {
    echo "{'success': 'true'}";
    unlink('./login');
} else {
    echo "{'success': 'false'}";
}
