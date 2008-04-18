<?php
// $Id: date_api_sql.inc,v 1.4.2.5 2008/04/18 20:48:13 karens Exp $

/**
 * Helper function to test if the database is set up to do timezone
 * conversions natively.
 */
function date_sql_db_timezone_support() {
  global $db_type;

  switch ($db_type) {
    case 'mysql':
    case 'mysqli':
      // If the timezone tables are empty, there is no support.
      $tables = db_result(db_query("SELECT COUNT(*) FROM mysql.time_zone_name;"));
      // Make sure the function returns a valid value and the data is current.
      $test = db_result(db_query("SELECT CONVERT_TZ('2007-03-11 2:00:00','US/Eastern','US/Central')"));
      if (empty($tables)) {
        return FALSE;
      }
      elseif ($test == '2007-03-11 01:00:00') {
        return TRUE;
      }
      return FALSE;

    case 'postgres':
      // TODO, need to find a way to test this for sure in postgres, but it
      // seems to be the default situation.
      return TRUE;
  }
}

/**
 *  Cross-database date SQL wrapper function for fields that are not native
 *  datetime fields and databases that don't support native timezone handling.
 *  Allows use of normalized native date functions in both mysql and postgres.
 *  Designed to be extensible to other databases.
 *
 *  @param $result_type - NOW, DATE, YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, DOW, DOY, WEEK
 *  @param $field - the name of the date field to be analyzed
 *  @param $date_type - the type of date field being analyzed, DATE_UNIX or DATE_ISO
 *  @param $offset - timezone offset in seconds, can be either a value or fieldname
 *  @param $offset_op - the operation to perform on the offset, + or -
 *  @return a SQL statement appropriate for the $db_type
 *
 *  example:          date_sql('WEEK', 'MYFIELD', 'int', 'MYOFFSETFIELD', '+')
 *  mysql returns:    WEEK(FROM_UNIXTIME(MYFIELD) + SEC_TO_TIME(MYOFFSETFIELD), 3)
 *  postgres returns: EXTRACT(WEEK FROM((MYFIELD::ABSTIME) + INTERVAL MYOFFSETFIELD SECONDS))
 * 
 *  DEPRECATED -- now just a wrapper function to ensure backwards compatibility.
 */
function date_sql($result_type, $field, $date_type = DATE_UNIX, $offset = '', $offset_op = '+') {
  $field_type = $date_type == DATE_UNIX ? 'int' : 'iso';
  if (!empty($offset) && $offset_op = '-') {
    $offset = -$offset;
  }
  return date_sql_extract($result_type, $field, $field_type, $offset);
}

/**
 * Helper function to create cross-database SQL dates.
 *
 * @param $field
 *   The real table and field name, like 'tablename.fieldname'.
 * @param $field_type
 *   The type of date field, 'int' or 'datetime'.
 * @param $set_offset
 *   The name of a field that holds the timezone offset or a fixed timezone
 *   offset value. If not provided, the normal Drupal timezone handling
 *   will be used, i.e. $set_offset = 0 will make no timezone adjustment.
 * @return
 *   An appropriate SQL string for the db type and field type.
 */
function date_sql_field($field, $field_type = 'int', $set_offset = NULL) {
  if ($field == 'NOW()') {
      return $field;
  }
  $db_type = $GLOBALS['db_type'];
  $offset = $set_offset !== NULL ? $set_offset : _views_get_timezone();
  switch ($db_type) {
    case 'mysql':
    case 'mysqli':
      switch ($field_type) {
        case 'int':
          $field = "FROM_UNIXTIME($field)";
          break;
        case 'iso':
          $field = "STR_TO_DATE($field, '%Y-%m-%%%%dT%T')";
          break;
        case 'datetime':
          break;
      }
      if (!empty($offset)) {
        $field = "ADDTIME($field, SEC_TO_TIME($offset))";
      }
      return $field;
    case 'pgsql':
      switch ($field_type) {
        case 'int':
          $field = "$field::ABSTIME";
          break;
        case 'iso':
          $field = $field = "TO_DATE($field, 'FMYYYY-FMMM-FMDDTFMHH:FMMI:FMSS')";
          break;
        case 'datetime':
          break;
      }
      if (!empty($offset)) {
        $field = "($field + 'INTERVAL $offset SECONDS')";
      }
      return $field;
  }
}

/**
 * Helper function to create cross-database SQL date formatting.
 *
 * @param $format
 *   A format string for the result, like 'Y-m-d H:i:s'.
 * @param $field
 *   The real table and field name, like 'tablename.fieldname'.
 * @param $field_type
 *   The type of date field, 'int' or 'datetime'.
 * @param $set_offset
 *   The name of a field that holds the timezone offset or a fixed timezone
 *   offset value. If not provided, the normal Drupal timezone handling
 *   will be used, i.e. $set_offset = 0 will make no timezone adjustment.
 * @return
 *   An appropriate SQL string for the db type and field type.
 */
function date_sql_format($format, $field, $field_type = 'int', $set_offset = NULL) {
  $db_type = $GLOBALS['db_type'];
  $field = date_sql_field($field, $field_type, $set_offset);
  switch ($db_type) {
    case 'mysql':
    case 'mysqli':
      $replace = array(
        'Y' => '%Y',
        'm' => '%m',
        'd' => '%%d',
        'H' => '%H',
        'i' => '%i',
        's' => '%s',
        );
      $format = strtr($format, $replace);
      return "DATE_FORMAT($field, '$format')";
    case 'pgsql':
      $replace = array(
        'Y' => 'YY',
        'm' => 'MM',
        'd' => 'DD',
        'H' => 'HH24',
        'i' => 'MI',
        's' => 'SS',
        );
      $format = strtr($format, $replace);
      return "TO_CHAR($field, '$format')";
  }
}

/**
 *  Cross-database date SQL wrapper function for fields that are native
 *  datetime fields and databases that support native timezone handling.
 *  Allows use of normalized native date functions in both mysql and postgres.
 *  Designed to be extensible to other databases.
 *
 *  @param $result_type - NOW, DATE, YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, DOW, DOY, WEEK
 *  @param $field - the name of the date field to be analyzed
 *  @param $timezone_to - the name of the timezone to convert the date to
 *  @param $timezone_from - the timezone that the date in the db is stored in.
 *  @return a SQL statement appropriate for the $db_type
 */
function date_db_sql($result_type, $field, $timezone_to, $timezone_from = 'UTC') {
  global $db_type;

  switch ($db_type) {
    case('mysql'):
    case('mysqli'):
      $field = "(CONVERT_TZ('$field', '$timezone_from', '$timezone_to'))";
      break;
    case('pgsql'):
      $field = "(SELECT TIMESTAMP WITH TIME ZONE '$field $timezone_from' AT TIME ZONE '$timezone_to')";
      break;
  }
  return date_sql_extract($result_type, $field);
}


/**
 *  Cross-database method of extracting date information from datetime fields.
 *  Allows use of normalized native date functions in both mysql and postgres.
 *  Designed to be extensible to other databases.
 *
 *  @param $result_type - NOW, DATE, YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, DOW, DOY, WEEK
 *  @param $field - the name of the date field to be analyzed
 *  @return a SQL statement appropriate for the $db_type
 */
function date_sql_extract($result_type, $field, $field_type = 'int', $set_offset = NULL) {
  global $db_type;
  
  $field = date_sql_field($field, $field_type, $set_offset);
  
  // Return requested sql.
  // Note there is no space after FROM to avoid db_rewrite problems
  // see http://drupal.org/node/79904.
  switch ($result_type) {
  case('NOW'):
  case('DATE'):
    return $field;
  case('YEAR'):
    return "EXTRACT(YEAR FROM($field))";
  case('MONTH'):
    return "EXTRACT(MONTH FROM($field))";
  case('DAY'):
    return "EXTRACT(DAY FROM($field))";
  case('HOUR'):
    return "EXTRACT(HOUR FROM($field))";
  case('MINUTE'):
    return "EXTRACT(MINUTE FROM($field))";
  case('SECOND'):
    return "EXTRACT(SECOND FROM($field))";
  case('WEEK'):  // ISO week number for date
    switch ($db_type) {
      case('mysql'):
      case('mysqli'):
        // WEEK using arg 3 in mysql should return the same value as postgres EXTRACT
        return "WEEK($field, 3)";
      case('pgsql'):
        return "EXTRACT(WEEK FROM($field))";
    }
  case('DOW'):
    switch ($db_type) {
      case('mysql'):
      case('mysqli'):
        // mysql returns 1 for Sunday through 7 for Saturday
        // php date functions and postgres use 0 for Sunday and 6 for Saturday
        return "INTEGER(DAYOFWEEK($field) - 1)";
      case('pgsql'):
        return "EXTRACT(DOW FROM($field))";
    }
  case('DOY'):
    switch ($db_type) {
      case('mysql'):
      case('mysqli'):
        return "DAYOFYEAR($field)";
      case('pgsql'):
        return "EXTRACT(DOY FROM($field))";
    }
  }
}

/**
 *  A helper function to do cross-database concatation of date parts
 *
 *  @param $array - an array of values to be concatonated in sql
 *  @return - correct sql string for database type
 */
function date_sql_concat($array) {
  global $db_type;

  switch ($db_type) {
    case('mysql'):
    case('mysqli'):
      return "CONCAT(". implode(",", $array) .")";
    case('pgsql'):
      return implode(" || ", $array);
  }
}

/**
 *  A helper function to do cross-database padding of date parts
 *
 *  @param $str - a string to apply padding to
 *  @param $size - the size the final string should be
 *  @param $pad - the value to pad the string with
 *  @param $side - the side of the string to pad
 */
function date_sql_pad($str, $size = 2, $pad = '0', $side = 'l') {
  switch ($side) {
  case('r'):
    return "RPAD($str, $size, '$pad')";
  default:
    return "LPAD($str, $size, '$pad')";
  }
}

/**
 *  Server timezone adjustment.
 *
 *  Used to compute default timezone adjustment made by SQL server
 *  so server adjustment can be removed and replaced with correct timezone
 *  adjustment values.
 *
 *  @return amount in seconds that DB server adjusts for timezone.
 */
function date_server_zone_adj() {
  global $db_type;
  static $server_zone_adj;
  if (!isset($server_zone_adj)) {
    switch ($db_type) {
      case('mysql'):
      case('mysqli'):
        // In 4.1.1+ we can use difference between NOW() and UTC_TIMESTAMP().
        if (version_compare(db_version(), '4.1.1', '>=')) {
          $server_zone_adj = db_result(db_query("SELECT UNIX_TIMESTAMP(NOW()) - UNIX_TIMESTAMP(UTC_TIMESTAMP())"));
        }
        break;
      case('pgsql'):
        // EXTRACT TIMEZONE returns the timezone adjustment in seconds.
        $server_zone_adj = db_result(db_query("SELECT EXTRACT('TIMEZONE' FROM NOW())"));
        break;
    }
    // If no value got set by this point,
    // fall back to using php timezone adjustment as an estimate.
    if (!isset($server_zone_adj)) {
      $server_zone_adj = date('Z');
    }
  }
  return $server_zone_adj;
}