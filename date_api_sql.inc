<?php
// $Id: date_api_sql.inc,v 1.4.2.7 2008/05/09 17:23:23 karens Exp $

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
 * A class to manipulate date SQL.
 * 
 * TODO
 *  do more debugging for situations when dates are stored in something other than UTC.
 */
class date_sql_handler {
  var $db_type = 'mysql';
  var $date_type = 'int';
  var $db_timezone = 'UTC';
  var $local_timezone = NULL;
  
  function construct($date_type = 'int', $local_timezone = NULL) {
    $this->db_type = $GLOBALS['db_type'];
    $this->date_type = $date_type;
    $this->db_timezone = 'UTC';
    $this->local_timezone = isset($local_timezone) ? $local_timezone : $this->get_timezone();
    date_api_set_db_timezone();
  }

  /**
   * See if the db has timezone name support.
   */
  function db_tz_support() {
    $has_support = variable_get('date_db_tz_support', -1);
    if ($has_support == -1) {
      date_api_set_db_timezone();
      $has_support = FALSE;
      switch ($this->db_type) {
        case 'mysql':
        case 'mysqli':
          $test = db_result(db_query("SELECT CONVERT_TZ('2008-02-15 12:00:00', 'UTC', 'US/Central')"));
          if ($test == '2008-02-15 06:00:00') {
            $has_support = TRUE;
          }
          break;
        case 'pgsql':
          $test = "TIMESTAMP WITH TIME ZONE '2008-02-15 12:00:00' AT TIME ZONE 'US/Central'";
          if ($test == '2008-02-15 06:00:00') {
            $has_support = TRUE;
          }
        break;
      }
      variable_set('date_db_tz_support', $has_support);
    }
    return $has_support;
  }

  /**
   * Return timezone offset.
   */
  function get_offset() {
    global $user;
    if (variable_get('configurable_timezones', 1) && $user->uid && strlen($user->timezone)) {
      $timezone = $user->timezone;
    }
    else {
      $timezone = variable_get('date_default_timezone', 0);
    }
    return $timezone;
  }
  
  /**
   * Override this to provide timezone name support.
   */
  function get_timezone() {
    global $user;
    if (variable_get('configurable_timezones', 1) && $user->uid && strlen($user->timezone_name)) {
      $timezone = $user->timezone_name;
    }
    else {
      $timezone = variable_get('date_default_timezone_name', NULL);
    }
    return $timezone;
  }

  /**
   * Helper function to create cross-database SQL dates.
   *
   * @param $field
   *   The real table and field name, like 'tablename.fieldname'.
   * @param $offset
   *   The name of a field that holds the timezone offset or an
   *   offset value. If NULL, the normal Drupal timezone handling
   *   will be used, if $offset = 0 no adjustment will be made.
   * @return
   *   An appropriate SQL string for the db type and field type.
   */
  function sql_field($field, $offset = NULL) {
    if (strtoupper($field) == 'NOW') {
      // NOW() will be in UTC since that is what we set the db timezone to.
      $this->local_timezone = 'UTC';
      return 'NOW()';
    }
    switch ($this->db_type) {
      case 'mysql':
      case 'mysqli':
        switch ($this->date_type) {
          case 'int':
            $field = "FROM_UNIXTIME($field)";
            break;
          case 'iso':
            $field = "STR_TO_DATE($field, '%Y-%m-%%dT%T')";
            break;
          case 'datetime':
            break;
        }
        break;
      case 'pgsql':
        switch ($date_type) {
          case 'int':
            $field = "$field::ABSTIME";
            break;
          case 'iso':
            $field = "TO_DATE($field, 'FMYYYY-FMMM-FMDDTFMHH:FMMI:FMSS')";
            break;
          case 'datetime':
            break;
        }
      break;
    }
    return $this->sql_tz($field, $offset);
  }

  /**
   * Select a date value from the database, adjusting the value
   * for the timezone.
   * 
   * Check whether database timezone conversion is supported in
   * this system and use it if possible, otherwise use an
   * offset.
   * 
   * @param $offset
   *   Set a fixed offset to use for the date. If set, no timezone
   *   conversion will be done and the offset will be used.
   */
  function sql_tz($field, $offset = NULL) {
    if (!$this->db_tz_support() || $offset !== NULL) {
      if ($offset === NULL) {
        $offset = $this->get_offset();
      }
      return $this->sql_offset($field, $offset);
    }
    switch ($this->db_type) {
      case 'mysql':
      case 'mysqli':
        return "CONVERT_TZ($field, $this->db_timezone, $this->local_timezone)";
      case 'pgsql':
        // WITH TIME ZONE assumes the date is using the system
        // timezone, which should have been set to UTC.
        return "TIMESTAMP WITH TIME ZONE $field AT TIME ZONE $this->local_timezone";
    }
  }
  
  /**
   * Adjust a field value by an offset in seconds.
   */
  function sql_offset($field, $offset = NULL) {
    if (!empty($offset)) {
      switch ($this->db_type) {
        case 'mysql':
        case 'mysqli':
          return "ADDTIME($field, SEC_TO_TIME($offset))";
        case 'pgsql':
          return "($field + 'INTERVAL $offset SECONDS')";;
      }
    }
    return $field;
  }
  
  /**
   * Helper function to create cross-database SQL date formatting.
   *
   * @param $format
   *   A format string for the result, like 'Y-m-d H:i:s'.
   * @param $field
   *   The real table and field name, like 'tablename.fieldname'.
   * @return
   *   An appropriate SQL string for the db type and field type.
   */
  function sql_format($format, $field) {
    switch ($this->db_type) {
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
   * Helper function to create cross-database SQL date extraction.
   *
   * @param $extract_type
   *   The type of value to extract from the date, like 'MONTH'.
   * @param $field
   *   The real table and field name, like 'tablename.fieldname'.
   * @return
   *   An appropriate SQL string for the db type and field type.
   */
  function sql_extract($extract_type, $field) {
    // Note there is no space after FROM to avoid db_rewrite problems
    // see http://drupal.org/node/79904.
    switch (strtoupper($extract_type)) {
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
      switch ($this->db_type) {
        case('mysql'):
        case('mysqli'):
          // WEEK using arg 3 in mysql should return the same value as postgres EXTRACT
          return "WEEK($field, 3)";
        case('pgsql'):
          return "EXTRACT(WEEK FROM($field))";
      }
    case('DOW'):
      switch ($this->db_type) {
        case('mysql'):
        case('mysqli'):
          // mysql returns 1 for Sunday through 7 for Saturday
          // php date functions and postgres use 0 for Sunday and 6 for Saturday
          return "INTEGER(DAYOFWEEK($field) - 1)";
        case('pgsql'):
          return "EXTRACT(DOW FROM($field))";
      }
    case('DOY'):
      switch ($this->db_type) {
        case('mysql'):
        case('mysqli'):
          return "DAYOFYEAR($field)";
        case('pgsql'):
          return "EXTRACT(DOY FROM($field))";
      }
    }
  }
  
  /**
   * Create a where clause to compare a complete date field to a complete date value.
   *
   * @param string $field
   *   The db table and field name, like "$table.$field".
   * @param string $operator
   *   The db comparison operator to use, like '='.
   * @param int $value
   *   The value to compare the extracted date part to, could be a
   *   field name or a date string or NOW().
   * @return 
   *   SQL for the where clause for this operation.
   */
  function sql_where_date($type, $field, $operator, $value, $adjustment = 0) {
    $type = strtoupper($type);
    if (strtoupper($value) == 'NOW') {
      $value = $this->sql_field('NOW', $adjustment);
    }
    elseif ($type == 'FIELD') {
      $value = $this->sql_field($value, $adjustment);
    }    
    elseif ($type == 'DATE') {
      $date = date_make_date($value, date_default_timezone_name(), DATE_DATETIME);
      if (!empty($adjustment)) {
        date_modify($date, $adjustment .' seconds');
      }
      $value = "'". date_format_date($date, 'custom', DATE_FORMAT_DATETIME) ."'";
    }    
    if ($this->local_timezone != $this->db_timezone) {
      $field = $this->sql_field($field);
    }
    else {
      $field = $this->sql_field($field, 0);
    }
    return "$field $operator $value";
  }
  
  /**
   * Create a where clause to compare an extracted part of a field to an integer value.
   *
   * @param string $part
   *   The part to extract, YEAR, MONTH, DAY, etc.
   * @param string $field
   *   The db table and field name, like "$table.$field".
   * @param string $operator
   *   The db comparison operator to use, like '='.
   * @param int $value
   *   The integer value to compare the extracted date part to.
   * @return 
   *   SQL for the where clause for this operation.
   */
  function sql_where_extract($part, $field, $operator, $value) {
    if ($this->local_timezone != $this->db_timezone) {
      $field = $this->sql_field($field);
    }
    else {
      $field = $this->sql_field($field, 0);
    }
    return $this->sql_extract($part, $field) ." $operator $value";
  }
  
  /**
   * Create a where clause to compare a formated field to a formated value.
   *
   * @param string $format
   *   The format to use on the date and the value when comparing them.
   * @param string $field
   *   The db table and field name, like "$table.$field".
   * @param string $operator
   *   The db comparison operator to use, like '='.
   * @param string $value
   *   The value to compare the extracted date part to, could be a
   *   field name or a date string or NOW().
   * @return 
   *   SQL for the where clause for this operation.
   */
  function sql_where_format($format, $field, $operator, $value) {
    if ($this->local_timezone != $this->db_timezone) {
      $field = $this->sql_field($field);
    }
    else {
      $field = $this->sql_field($field, 0);
    }
    return $this->sql_format($format, $field) ." $operator '". $this->sql_format($format, $value) ."'";
  }
}
