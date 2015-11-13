<?php
/**
 * Created by PhpStorm.
 * Date: 11/13/15
 * Time: 14:21
 */
header('Content-Type: application/json');
if (file_exists("./login")) {
    echo "{'ready': true, 'url': '/accept.php'}";
} else {
    echo "{'ready': false}";
}
