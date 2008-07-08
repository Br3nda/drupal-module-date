<?php
// $Id: date_views.inc,v 1.26.2.27 2008/07/08 23:55:53 karens Exp $

/**
 * Implementation of hook_views_style_plugins()
 */
function _date_views_style_plugins() {

  $items = array();
  $items['date_views_browser'] = array(
    'name' => t('Date: Date Browser'),
    'theme' => 'date_views_browser_full_view',
    'summary_theme' => 'date_views_browser_summary_view',
    'needs_fields' => true,
    'needs_table_header' => true,
    'validate' => 'date_browser_validate',
    'even_empty' => true,
  );
  return $items;
}

/**
 * Views field filters
 */
function _date_views_filters($field) {
  switch ($field['type']) {
    case(DATE_ISO):
      $handler = 'date_views_filter_handler';
      $ymd_handler = 'date_views_handler_filter_ymd';
      break;
    case(DATE_UNIX):
      $handler = 'date_views_timestamp_filter_handler';
      $ymd_handler = 'date_views_timestamp_handler_filter_ymd';
      break;
  }
  // The $field array is missing widget info, get the complete widget
  // with the format settings.
  $types = content_types();
  $widget = $types[$field['type_name']]['fields'][$field['field_name']]['widget'];
  $format = !empty($widget['input_format_custom']) ? $widget['input_format_custom'] : $widget['input_format'];

  // use this to default to current time
  $current = array('' => t('<all>'), 'now' => t('now'));
  $months  = ($current + drupal_map_assoc(range(1, 12), 'map_month'));
  $days    = ($current + drupal_map_assoc(range(1, 31)));

  $operator = array(
        '=' => t('is equal to'),
        '<>' => t('is not equal to'),
        '>' => t('greater than'),
        '>=' => t('greater than or equal to'),
        '<' => t('less than'),
        '<=' => t('less than or equal to'),
        );
  $description = 'Filter by %option. Set a default date and time in the <strong>Value</strong>. To default to the current time instead of a fixed time, leave the <strong>Value</strong> empty and enter \'now\' in the <strong>Option</strong>. You may also use something like \'now +1 day\' to default to one day from the current time.';
  $filter = array(
    'operator' => $operator,
    'option' => 'string',
    'handler' => $handler,
    'extra' => array('column' => 'value', 'field' => $field),
    'cacheable' => 'no',
  );
  $filters = array(
    'default' => $filter + array(
      'name' => t('Date'),
      'value' => date_views_handler_filter_date_value_form($field),
      'type' => 'DATE',
      'help' => t($description, array('%option' => t('date'))),
      ),
    'year' => $filter + array(
      'name' => t('Year'),
      'type' => 'YEAR',
      'help' => t($description, array('%option' => t('year'))),
      ),
    'month' => $filter + array(
      'name' => t('Month'),
      'list' => $months,
      'list-type' => 'select',
      'type' => 'MONTH',
      'help' => t($description, array('%option' => t('month'))),
      ),
    'day' => $filter + array(
      'name' => t('Day'),
      'list' => $days,
      'list-type' => 'select',
      'type' => 'DAY',
      'help' => t($description, array('%option' => t('day'))),
      ),
    );

    // Differentiate from and to dates with a prefix that is not likely to ever be used normally.
    if ($field['todate']) {
      $filters2['to|default'] = $filters['default'];
      $filters2['to|default']['name'] = t('To Date');
      $filters2['to|default']['extra'] = array('column' => 'value2', 'field' => $field);
      $filters2['to|year'] = $filters['year'];
      $filters2['to|year']['name'] = t('To Year');
      $filters2['to|year']['extra'] = array('column' => 'value2', 'field' => $field);
      $filters2['to|month'] = $filters['month'];
      $filters2['to|month']['name'] = t('To Month');
      $filters2['to|month']['extra'] = array('column' => 'value2', 'field' => $field);
      $filters2['to|day'] = $filters['day'];
      $filters2['to|day']['name'] = t('To Day');
      $filters2['to|day']['extra'] = array('column' => 'value2', 'field' => $field);
      $filters += $filters2;
    }
    return $filters;
}

/**
 * Views arguments for Date fields.
 */
function _date_views_arguments($field) {
  $field_types = _content_field_types();
  $arguments = array();
  $argument = array();
  $argument['name'] = $field_types[$field['type']]['label'] . ($field['todate'] ? t(': From ') : ': ') . t($field['widget']['label']) .' ('. $field['field_name'] .')';
  $argument['handler'] = $field['type'] == 'date' ? 'date_views_argument_range_handler' : 'date_views_timestamp_argument_range_handler';
  $argument['help'] = t("Defines an argument to filter for dates within a range, in the format 'YYYY-MM-DD--YYYY-MM-DD'. Many other options can be used in arguments. See !link for other examples.", array('!link' => l(t('help'), 'admin/help/date')));
  $argument['option'] = 'date_range_arg_options';
  $arguments['content: '. $field['field_name']] = $argument;

  if ($field['todate']) {
    $argument['name'] = $field_types[$field['type']]['label'] . t(': To ') . t($field['widget']['label']) .' ('. $field['field_name'] .')';
    $arguments['content: to|'. $field['field_name']] = $argument;
  }
  return $arguments;
}

/**
 *  Handler for date filter.
 */
function _date_views_timestamp_filter_handler($op, $filter, $filterinfo, &$query) {
  // this is just a wrapper function that sets the date type
  return _date_views_filter_handler($op, $filter, $filterinfo, $query, DATE_UNIX);
 }
function _date_views_filter_handler($op, $filter, $filterinfo, &$query, $field_type = DATE_ISO) {
  // Filter out 5.1 version values that won't work in 5.2.
  if (isset($filter['value']['date']) && !preg_match(DATE_REGEX_LOOSE, $filter['value']['date'])) {
    $filter['value'] = '';
  }
  if (empty($filter['value']) && empty($filter['options'])) {
    return;
  }
  include_once('./'. drupal_get_path('module', 'date_api') .'/date_api_sql.inc');
  include_once('./'. drupal_get_path('module', 'date_api') .'/date_api_elements.inc');
  $field   = $filterinfo['extra']['field'];
  $column = $filterinfo['extra']['column'];
  $db_info = $filterinfo['content_db_info'];
  $table   = 'node_data_'. $field['field_name'];
  $sql_field = "$table.". $db_info['columns'][$column]['column'];

  $timezone   = $db_info['columns']['timezone']['column'];
  $offset     = $db_info['columns']['offset']['column'];
  $table      = 'node_data_'. $field['field_name'];
  
  $date_handler = new date_sql_handler();
  $date_handler->construct($field['type']);
  if ($field['tz_handling'] == 'date') {
    $date_handler->local_timezone_field = "$table.$timezone";
    $date_handler->local_offset_field = "$table.$offset";
  }
  else {
    $timezone = date_default_timezone_name();
  }
  date_views_set_timezone($date_handler, $query, $field);
  
  // We're comparing to a local time, 
  // so make sure the date_hander knows that.
  if ($date_handler->db_timezone == 'UTC' && empty($date_handler->local_timezone_field)) {
    $date_handler->local_timezone = date_default_timezone_name();
  }
      
  $sql = '';  
  $adjustment = trim(str_replace('now', '', $filter['options']));

  // Comparing to NOW.
  if (empty($filter['value']) && !empty($filter['options'])) {
    $date = date_now();
    if (!empty($adjustment)){
      date_modify($date, $adjustment);
    }
    switch ($filterinfo['type']) {
      case 'YEAR':
      case 'MONTH':
      case 'DAY':
        $formats = array('YEAR' => 'Y', 'MONTH' => 'n', 'DAY' => 'j');
        $value = date_format($date, $formats[$filterinfo['type']]);
        $sql = $date_handler->sql_where_extract($filterinfo['type'], $sql_field, $filter['operator'], $value, FALSE);
        break;
      default:
        $granularity = array_pop($field['granularity']);
        $format = $date_handler->views_formats($granularity, 'sql');
        $sql = $date_handler->sql_where_format($format, $sql_field, $filter['operator'], date_format($date, $format));
        break;
    }
  }
  // Comparing to an input date.
  elseif ($filterinfo['type'] == 'DATE') {
    // If the date_popup or date_textfield elements are used as a exposed filters, their
    // #validation functions will never get called, so validate and
    // reconstruct them here.
    if (is_array($filter['value'])) {
      $input = $filter['value']['date'];
      if (!empty($filter['value']['time'])) {
        $input .= ' '. $filter['value']['time'] .':00';
      }
      elseif (!empty($input)) {
        $input .= ' 00:00:00';
      }
    }
    else {
      $input = $filter['value'];
    }
    if (!empty($input)) {
      if (date_is_valid($input, DATE_DATETIME)) {
        $date = date_make_date($input, date_default_timezone_name(), DATE_DATETIME);
        if (!empty($adjustment)) {
          date_modify($date, $adjustment);
        }
        $formats = array('YEAR' => 'Y', 'MONTH' => 'n', 'DAY' => 'j', 'DATE' => DATE_FORMAT_DATETIME);
        $value = date_format_date($date, 'custom', $formats[$filterinfo['type']]);
        $sql = $date_handler->sql_where_date('DATE', $sql_field, $filter['operator'], $value);
      }
      else {
        drupal_set_message(t('That is not a valid date.'));
      }
    }
  }
  // Comparing to a YEAR, MONTH, or DAY.
  else {
    $value = $filter['value'];
    $sql = $date_handler->sql_where_extract($filterinfo['type'], $sql_field, $filter['operator'], $value);
  }
  if (!empty($sql)) {
    $query->ensure_table($table);
    $query->add_where($sql);
  }
}

/**
 * Provide a form gadget for dates.
 */
function _date_views_handler_filter_date_value_form($field) {
  include_once(drupal_get_path('module', 'date_api') .'/date_api_elements.inc');
  $form = array(
    '#type' => module_exists('date_popup') ? 'date_popup' : 'date_text',
    '#date_timezone' => date_default_timezone_name(),
    '#date_format' => 'Y-m-d H:i',
    '#size' => 20,
  );
  return $form;
}

/**
 *
 * Flexible date range argument handler
 */
function _date_views_timestamp_argument_range_handler($op, &$query, $argtype, $arg = '') {
  // this is just a wrapper function that sets the date type
  return _date_views_argument_range_handler($op, $query, $argtype, $arg, DATE_UNIX);
}
function _date_views_argument_range_handler($op, &$query, $argtype, $arg = '', $field_type = DATE_ISO) {
  static $format;

  include_once('./'. drupal_get_path('module', 'date_api') .'/date_api_sql.inc');

  $name = explode(':', is_array($argtype) ? $argtype['type'] : $argtype);
  $tofield_name = trim($name[1]);
  $field_name = drupal_substr($tofield_name, 0, 3) == 'to|' ? drupal_substr($tofield_name, 3) : $tofield_name;
  if ($field_name == $tofield_name) {
    $value = 'value';
  }
  else {
    $value = 'value2';
  }

  $field      = content_fields($field_name);
  $db_info    = content_database_info($field);
  $value      = $db_info['columns'][$value]['column'];
  $timezone   = $db_info['columns']['timezone']['column'];
  $offset     = $db_info['columns']['offset']['column'];
  $table      = 'node_data_'. $field['field_name'];
  $sql_field = "$table.$value";
  
  $date_handler = new date_sql_handler();
  $date_handler->construct($field['type']);
  if ($field['tz_handling'] == 'date') {
    $date_handler->local_timezone_field = "$table.$timezone";
    $date_handler->local_offset_field = "$table.$offset";
  }
  if (!empty($arg)) {
    $granularity = $date_handler->arg_granularity($arg);
    $format = $date_handler->views_formats($granularity, 'display');
    $sql_format = $date_handler->views_formats($granularity, 'sql');
    $range = $date_handler->arg_range($arg);
    $query->min_date = $range[0];
    $query->max_date = $range[1];
    date_views_set_timezone($date_handler, $query, $field);
    $query->date_handler = $date_handler;
  }
  else {
    $granularity = $date_handler->arg_granularity($query);
    $format = $date_handler->views_formats($granularity, 'display');
    $sql_format = $date_handler->views_formats($granularity, 'sql');
    $range = $date_handler->arg_range($query);
  }
  
  switch ($op) {
    case 'summary':
      $fieldinfo['field'] = $date_handler->sql_format($sql_format, $date_handler->sql_field($sql_field));
      $fieldinfo['fieldname'] = 'range';
      
      $query->ensure_table($table);
      $query->add_field($value, $table);
      if ($field['tz_handling'] == 'date') {
        $query->add_field($timezone, $table);
      }
      return $fieldinfo;
      break;

    case 'link':
      // links are created from date range groupings
      if (empty($query->$value)) return;
      if ($field['tz_handling'] == 'date') {
        $tz_name = $query->$timezone;
      }
      else {
        $tz_name = date_default_timezone_name();
      }
      $date = date_make_date($query->$value, date_default_timezone_name(), $field_type);
      if (!empty($tz_name)) {
        date_timezone_set($date, timezone_open($tz_name));
      }
      return l(date_format_date($date, 'custom', $format), $arg .'/'. $query->range);

    case 'sort':
      break;

    case 'filter':
      $query->ensure_table($table);
      $query->add_field($value, $table);
      if ($field['tz_handling'] == 'date') {
        $query->add_field($timezone, $table);
      }
      $query->add_where($date_handler->sql_where_date('DATE', $sql_field, '>=', date_format($date_handler->min_date, DATE_FORMAT_DATETIME)));
      $query->add_where($date_handler->sql_where_date('DATE', $sql_field, '<=', date_format($date_handler->max_date, DATE_FORMAT_DATETIME)));
      break;

    case 'title':
      return date_format($range[0], $format);
  }
}

/**
 *  Define groupby options for date range summaries
 */
function date_range_arg_options() {
  return array(
    'year'  => t('summarize by year'),
    'month' => t('summarize by month'),
    'day'   => t('summarize by day'),
    'week'  => t('summarize by week'),
    'hour'  => t('summarize by hour')
    );
}

/**
 * Validate a view.
 */
function date_browser_validate($type, $view, $form) {
  // list (and table) modes require there to be at least 1 field active.
  if (is_array($view['field'])) {
    $fields = array_filter(array_keys($view['field']), 'is_numeric');
  }
  if (!$fields) {
    form_error($form["$type-info"][$type .'_type'], t('The Date Browser requires at least one field.'));
  }
  // Make sure all arguments are set to 'Display all values'
  // and that a date argument has been provided.
  $found = FALSE;
  $options = array_keys(date_range_arg_options());
  foreach ($view['argument'] as $delta => $argument) {
    if (in_array($argument['options'], $options)) {
      $found = TRUE;
      if (is_numeric($delta) && $argument['argdefault'] != 2) {
        form_error($form['argument'][$delta]['argdefault'], t('Date Browser arguments must be set to \'Display All Values\'.'));
      }
    }
  }
  if (!$found) {
    form_error($form['argument'], t('A date argument must be added to a Date Browser view.'));
  }
}

/**
 *  Implementation of hook_views_query()
 *  Used to make sure view defaults to current date if no date selected
 */
function _date_views_query_alter(&$query, &$view) {
  include_once('./'. drupal_get_path('module', 'date_api') .'/date_api_sql.inc');
  $date_views_browser_views = date_views_browser_get_views();
  if (in_array($view->name, array_keys($date_views_browser_views))) {
    
    $name       = explode(':', $view->argument[0]['type']);
    $tofield_name = trim($name[1]);
    $field_name = drupal_substr($tofield_name, 0, 3) == 'to|' ? drupal_substr($tofield_name, 3) : $tofield_name;
    $value = $field_name != $tofield_name ? 'value2' : 'value';
    $field      = content_fields($field_name);
    $db_info    = content_database_info($field);
    $table      = 'node_data_'. $field['field_name'];
    
    $date_handler = new date_sql_handler();
    $date_handler->construct($field['type']);
    if ($field['tz_handling'] == 'date') {
      $date_handler->local_timezone_field = "$table.$timezone";
      $date_handler->local_offset_field = "$table.$offset";
    }
    date_views_set_timezone($date_handler, $query, $field);
    $view->date_handler = $date_handler;

    // Add a combo FROM|TO date field to the query
    $value      = $db_info['columns']['value']['column'];
    $value2     = !empty($db_info['columns']['value2']['column']) ? $db_info['columns']['value2']['column'] : $db_info['columns']['value']['column'];
    $combo = date_sql_concat(array($table .'.'. $value, "'|'", $table .'.'. $value2)) .' AS date_combo ';
    $query->add_field($combo, NULL);
    $path = explode('/', $view->url);
    $pos = sizeof($path);
    if ($view->build_type == 'block' || arg($pos) == '') {
      $arg = NULL;
    }
    else {
      $arg = arg($pos);
    }
    
    // If no argument is specified, the argument handling won't get executed.
    // Add the current date range to the query.
    if ($arg == NULL) {
      $period = $view->argument[0]['options'];
      $format = $date_handler->views_formats($period, 'sql');
      $arg = date_format_date(date_now(), 'custom', $format) . date_views_browser_period($period);
      if ($range  = $view->date_handler->arg_range($arg)) {
        $query->ensure_table($table);
        $query->add_field('nid', 'node');
        $query->add_field($value, $table);
        $sql_field = "$table.$value";
        $query->add_where($date_handler->sql_where_date('DATE', $sql_field, ">=", date_format($range[0], DATE_FORMAT_DATETIME)), $offset);
        $query->add_where($date_handler->sql_where_date('DATE', $sql_field, "<=", date_format($range[1], DATE_FORMAT_DATETIME)), $offset);
      }
    }
  }
}

/**
 * Find all the views that qualify for date browser treatment
 *
 * @param $view
 *   if called from Views UI validation, $edit_view will be populated,
 *   otherwise it will be empty. Use that to tell if the list
 *   needs to be refreshed.
 */
function date_views_browser_get_views($reset = FALSE) {
  static $date_views_browser_views;
  if (empty($date_views_browser_views) || $reset) {
    $cid = 'date_browser_views';
    if (!$reset && $cached = cache_get($cid, 'cache_views')) {
      $date_views_browser_views = unserialize($cached->data);
    }
    else {
      $date_views_browser_views = array();
      $arguments = array();
      $fields = content_fields();
      foreach ($fields as $field) {
        if ($field['type'] == DATE_UNIX || $field['type'] == DATE_ISO) {
          $arguments = array_merge($arguments, _date_views_arguments($field));
        }
      }
      $argument_list = "'". implode("','", array_keys($arguments)) ."'";
      if (!$argument_list) {
        return array();
      }
      $result = db_query("SELECT arg.*, view.name FROM {view_argument} arg INNER JOIN {view_view} view ON arg.vid=view.vid WHERE arg.type IN ($argument_list) AND view.page_type='date_views_browser'");
      while ($view = db_fetch_object($result)) {
        $date_views_browser_views[$view->name] = $view;
      }
      cache_set($cid, 'cache_views', serialize($date_views_browser_views));
    }
  }
  return $date_views_browser_views;
}

/**
 *  Return the correct period for the date range argument
 */
function date_views_browser_period($period = 'month') {
  switch ($period) {
  case('year'):
    return 'P1Y';
  case('week'):
    return 'P1W';
  case('day'):
    return 'P1D';
  case('hour'):
    return 'P1H';
  default:
    return 'P1M';
  }
}

/**
 *  Navigation links for the full view
 */
function date_views_browser_navigation($view, $period) {
  $arg = NULL;
  foreach ($view->argument as $pos => $argument) {
    if ($argument['options'] == $period) {
      $arg = $view->args[$pos];
    }
  }
  
  // Create a date object for the beginning of the current range.
  $range = $view->date_handler->arg_range($arg);
  $format = $view->date_handler->views_formats($period, 'sql');
  $date = drupal_clone($range[0]);
    
  // Move backwards 1 period and create a link to that date.
  date_modify($date, '-1'. $period);
  $prev = $view->url .'/'. date_format_date($date, 'custom', $format) . date_views_browser_period($period);
  // Move forward 2 periods (back to the current period and to the next period)
  // and create a link to that date.
  date_modify($date, '+2 '. $period);
  $next = $view->url .'/'. date_format_date($date, 'custom', $format) . date_views_browser_period($period);

  $label = theme('date_views_browser_period_label', $period, $range[0]);
  return theme('date_views_browser_navigation', $label, $period, $prev, $next, $view);
}

/**
 * Central function for setting up the right timezone values
 * in the SQL date handler.
 * 
 * Min and max date are always available or important, but the
 * timezone is.
 */
function date_views_set_timezone(&$date_handler, &$view, $field) {
  $min_date = NULL;
  $max_date = NULL;
  $min_utc = NULL;
  $max_utc = NULL;
  if (isset($view->min_date) && isset($view->max_date)) {
    $min_date = $view->min_date;
    $max_date = $view->max_date;
    if (empty($view->min_utc)) {
      $view->min_utc = drupal_clone($min_date);
      date_timezone_set($view->min_utc, timezone_open('UTC'));
    }
    if (empty($view->max_utc)) {
      $view->max_utc = drupal_clone($max_date);
      date_timezone_set($view->max_utc, timezone_open('UTC'));   
    }
    $min_utc = $view->min_utc;
    $max_utc = $view->max_utc;
  }
  $tz_handling  = $field['tz_handling'];
  switch ($tz_handling) {
    case 'date' :
      $date_handler->db_timezone = 'UTC';
      $date_handler->local_timezone_field = $field['timezone_field'];
      $date_handler->local_offset_field = $field['offset_field'];
      $date_handler->min_date = $min_date;
      $date_handler->max_date = $max_date;
      break;
    case 'none':
      $date_handler->db_timezone = date_default_timezone_name();
      $date_handler->local_timezone = date_default_timezone_name();
      $date_handler->min_date = $min_date;
      $date_handler->max_date = $max_date;
      break;
    case 'utc':
      $date_handler->db_timezone = 'UTC';
      $date_handler->local_timezone = 'UTC';
      $date_handler->min_date = $min_utc;
      $date_handler->max_date = $max_utc;
      break;
    default :
      $date_handler->db_timezone = 'UTC';
      $date_handler->local_timezone = 'UTC';
      $date_handler->min_date = $min_utc;
      $date_handler->max_date = $max_utc;
      break;
  }
}