<?php
// $Id: date_views.inc,v 1.26.2.56 2009/03/27 10:26:27 karens Exp $

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
 * Switch out the regular CCK field handler to use a date version that
 * can be smarter about how to handle grouped dates.
 */
function date_views_field_tables($field) {
  $field_types = _content_field_types();
  $tables = content_views_field_tables($field);
  $key = 'node_data_'. $field['field_name'];

  $db_info = content_database_info($field);
  $columns = $db_info['columns'];
  $main_column = array_shift($columns);
  $tables[$key]['fields'][$main_column['column']]['handler'] = array(
    'date_views_field_handler_group' => t('Group multiple values'),
    'date_views_field_handler_ungroup' => t('Do not group multiple values'),
    'date_views_field_handler_first' => t('Show first value only'),
    'date_views_field_handler_last' => t('Show last value only'),
  );
  return $tables;
}

function date_views_field_handler_group($field_info, $field_data, $value, $data, $from = 0, $count = 'all', $order = 'ASC') {
  $view = $GLOBALS['current_view'];
  $handler = $field_data['handler'];
  $field = $field_info['content_field'];
  $items = array();
  if ($field['multiple']) {
    $table_alias = "node_data_$field[field_name]";
    foreach ($field_info['content_db_info']['columns'] as $column => $attributes) {
      $query_columns[] = "$table_alias.$attributes[column] AS $column";
    }
    $query_columns[] = "$table_alias.delta AS delta";
    
    // If the view has a minimum/maximum date, limited the grouped
    // values to the date range.
    $where = '';
    if (!empty($view->date_handler) && !empty($view->min_date)) {
      $date_handler = $view->date_handler;
      $sql_field = $field_data['fullname'];
      $replace = date_api_views_query_substitutions($view);
      $sql1 = $date_handler->sql_where_date('DATE', $sql_field, ">=", date_format($view->min_date, DATE_FORMAT_DATETIME));
      $sql2 = $date_handler->sql_where_date('DATE', $sql_field, "<=", date_format($view->max_date, DATE_FORMAT_DATETIME));
      $where = ' AND '. strtr($sql1, $replace) .' AND '. strtr($sql2, $replace);
    }
    
    // Note : this query doesn't need to run through db_rewrite_sql, since the
    // nids we retrieve have been selected by the views query, which already takes
    // care of this.
    $query = "SELECT ". implode(', ', $query_columns) .
             " FROM {node} node".
             " LEFT JOIN {". $field_info['content_db_info']['table'] ."} $table_alias ON node.vid = $table_alias.vid".
             " WHERE node.nid = ". $data->nid . $where .
             " ORDER BY $table_alias.delta $order";
    // Select all deltas or only a subset.
    // Currently only 'all' and first / last items are proposed to the user.
    $result = ($count == 'all') ? db_query($query) : db_query_range($query, $from, $count);

    while ($item = db_fetch_array($result)) {
      $item['#delta'] = $item[delta];
      unset($item['delta']);
      if ($field['repeat'] && $handler == 'date_views_field_handler_group') {
        $data->date_repeat_show = TRUE;
      }
      $items[] = content_format($field, $item, $field_data['options'], $data);
      
    }
    return theme('content_view_multiple_field', $items, $field, $data);
  }
  else {
    return date_views_field_handler_ungroup($field_info, $field_data, $value, $data);
  }
}

function date_views_field_handler_first($field_info, $field_data, $value, $data) {
  return date_views_field_handler_group($field_info, $field_data, $value, $data, 0, 1, 'ASC');
}

function date_views_field_handler_last($field_info, $field_data, $value, $data) {
  return date_views_field_handler_group($field_info, $field_data, $value, $data, 0, 1, 'DESC');
}

function date_views_field_handler_ungroup($field_info, $field_data, $value, $data) {
  $field = $field_info['content_field'];
  $item = array();
  foreach ($field_info['content_db_info']['columns'] as $column => $attributes) {
    $view_column_name = $field_data['tablename'] .'_'. $attributes['column'];
    $item[$column] = $data->$view_column_name;
  }
  $item['#delta'] = $data->{$field_data['tablename'] .'_delta'};
  return content_format($field, $item, $field_data['options'], $data);
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
  require_once('./'. drupal_get_path('module', 'date_api') .'/date_api_sql.inc');
  require_once('./'. drupal_get_path('module', 'date_api') .'/date_api_elements.inc');
  
  // There's no simple way to do this, figure out if this filter has an
  // exposed value, and if so, ignore options like 'now'. Need to do 
  // this so we don't adjust to 'now' when the date is deliberately
  // set to be empty in an exposed filter, while still using 'now'
  // as the default value when nothing has been submitted in the exposed
  // filter.
  $view = $GLOBALS['current_view'];
  foreach ((array) $view->exposed_filter as $delta => $exposed_filter) {
    $exposed_delta = trim(str_replace('filter', '', $name));
    $exposed_field = $view->exposed_filter[$exposed_delta]['field'];
    if ($filter['field'] == $exposed_field && $delta == $exposed_delta) {
      // This is an exposed filter that has been submitted.
      if (isset($_GET['filter'. $exposed_delta])) {
        $filter['options'] = '';
      }
    }
  }
        
  // $filter['value'] could either be an array of 'date' and 'time',
  // or an array of date parts, or a single value, depending on which 
  // widget was used. Normalize the values before analyzing them.
  $field     = $filterinfo['extra']['field'];
  $types     = content_types($field['type_name']);
  $field     = $types['fields'][$field['field_name']];
  $column    = $filterinfo['extra']['column'];
  $db_info   = $filterinfo['content_db_info'];
  
  // A submitted date_select will be an array of date parts.
  if (is_array($filter['value']) && array_key_exists('year', $filter['value'])) {
    $value = date_convert($filter['value'], DATE_ARRAY, DATE_DATETIME);
  }
  // A submitted date_popup will be an array of separate date and time.
  // A submitted date_text will be an array with a single complete date.
  elseif (is_array($filter['value']) && array_key_exists('date', $filter['value'])) {
    $format = !empty($field['widget']['input_format_custom']) ? $field['widget']['input_format_custom'] : $field['widget']['input_format'];
    $format = date_limit_format($format, date_granularity($field));  
    // If time is empty, make sure the custom format isn't expecting it.
    if (empty($filter['value']['time'])) {
      $format = date_limit_format($format, array('year', 'month', 'day'));  
    }
    $value = trim($filter['value']['date'] .' '. $filter['value']['time']);
    $value = date_convert_from_custom($value, $format);
  }
  // The default value is already in the datetime format.
  else {
    $value = $filter['value'];
  }
    
  // Filter out 5.1 version values that won't work in 5.2.
  if ($filterinfo['type'] == 'DATE' && !preg_match(DATE_REGEX_LOOSE, $value)) {
    $value = '';
  }
  if (empty($value) && empty($filter['options'])) {
    return;
  }
  $table     = 'node_data_'. $field['field_name'];
  $sql_field = "$table.". $db_info['columns'][$column]['column'];
  $table     = 'node_data_'. $field['field_name'];
  $date_handler = new date_sql_handler();
  $date_handler->construct($field['type']);
  $timezone = date_default_timezone_name();
  date_views_set_timezone($date_handler, $field);
      
  $sql = '';  
  $adjustment = trim(str_replace('now', '', $filter['options']));
      
  // Comparing to NOW.
  if (empty($value) && !empty($filter['options'])) {
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
        $granularity = $field['granularity'];
        $granularity = array_pop(array_filter($granularity));
        $format = $date_handler->views_formats($granularity, 'sql');
        $sql = $date_handler->sql_where_format($format, $sql_field, $filter['operator'], date_format($date, $format));
        break;
    }
  }
  // Comparing to an input date.
  elseif ($filterinfo['type'] == 'DATE') {
    if (!empty($value)) {
      if (date_is_valid($value, DATE_DATETIME, $field['granularity'])) {
        $date = date_make_date($value, date_default_timezone_name(), DATE_DATETIME, $field['granularity']);
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
    $sql = $date_handler->sql_where_extract($filterinfo['type'], $sql_field, $filter['operator'], $value);
  }
  if (!empty($sql)) {
    $query->ensure_table($table);
    $query->add_where($sql);
  }
}

/**
 * Provide a form gadget for dates.
 * 
 * Use the same form element used for the field.
 */
function _date_views_handler_filter_date_value_form($field) {
  $types = content_types($field['type_name']);
  $field = $types['fields'][$field['field_name']];
  $format = !empty($field['widget']['input_format_custom']) ? $field['widget']['input_format_custom'] : $field['widget']['input_format'];
  require_once('./'. drupal_get_path('module', 'date_api') .'/date_api_elements.inc');
  $form = array(
    '#type' => $field['widget']['type'],
    '#date_timezone' => date_default_timezone_name(),
    '#date_format' => date_limit_format($format, date_granularity($field)),
    '#date_text_parts' => (array) $field['widget']['text_parts'],
    '#date_increment' => $field['widget']['increment'],
    '#date_year_range' => $field['widget']['year_range'],
    '#date_label_position' => $field['widget']['label_position'],
    '#views_filter' => TRUE,
  );
  return $form;
}

/**
 * Flexible date range argument handler.
 * 
 * Views values vary depending on the op:
 * 
 * Summary views trigger: summary, sort, link.
 * Main views trigger: filter, title.
 */
function _date_views_timestamp_argument_range_handler($op, &$query, $argtype, $arg = '') {
  // this is just a wrapper function that sets the date type
  return _date_views_argument_range_handler($op, $query, $argtype, $arg, DATE_UNIX);
}
function _date_views_argument_range_handler($op, &$query, $argtype, $arg = '', $field_type = DATE_ISO) {
  require_once('./'. drupal_get_path('module', 'date_api') .'/date_api_sql.inc');
  // Switch to the function that matches the requested $op.
  $function = '_date_views_argument_'.$op;
  return $function($query, $argtype, $arg, $field_type);
}

/**
 * Views values vary depending on the op:
 * $op = 'filter'
 *   $argtype = array of arg settings
 *   $query = views object
 *   $arg = argument value
 */
function _date_views_argument_filter(&$query, $argtype, $arg, $field_type) {
  $field_name = _date_views_arg_field_name($argtype['type']);
  $field      = content_fields($field_name);
  $db_info    = content_database_info($field);
  $value      = substr($field_name, -1) == 2 ? 'value2' : 'value';
  $value      = $db_info['columns'][$value]['column'];
  $table      = 'node_data_'. $field['field_name'];
  $sql_field  = "$table.$value";
  $query->sql_field_name = $sql_field;

  $date_handler = new date_sql_handler();
  $date_handler->construct($field_type);
  $range = $date_handler->arg_range($arg);
  $date_handler->granularity = $date_handler->arg_granularity($arg);
  date_views_set_timezone($date_handler, $field);
  $query->date_handler = $date_handler;
  $query->ensure_table($table);
  $query->add_field($value, $table);
  if ($field['tz_handling'] == 'date') {
    $query->add_field($db_info['columns']['timezone']['column'], $table);
    $query->add_field($db_info['columns']['offset']['column'], $table);
  }
  if ($date_handler->granularity != 'week') {
    $format = $date_handler->views_formats($date_handler->granularity, 'sql');
    $query->add_where($date_handler->sql_where_format($format, $sql_field, '>=', date_format($range[0], $format)));
    $query->add_where($date_handler->sql_where_format($format, $sql_field, '<=', date_format($range[1], $format)));
  }
  else {
    $query->add_where($date_handler->sql_where_date('DATE', $sql_field, ">=", date_format($range[0], DATE_FORMAT_DATETIME)));
    $query->add_where($date_handler->sql_where_date('DATE', $sql_field, "<", date_format($range[1], DATE_FORMAT_DATETIME)));
  }
}

/**
 * Views values vary depending on the op:
 * $op = 'title'
 *   $argtype = name of the argument
 *   $query = argument value
 *   $arg = <empty>
 */
function _date_views_argument_title($query, $argtype, $arg, $field_type) {
  $date_handler = new date_sql_handler();
  $date_handler->construct($field_type);
  $granularity = $date_handler->arg_granularity($query);
  $format = $date_handler->views_formats($granularity, 'display');
  $range = $date_handler->arg_range($query);
  $formatted = date_format_date($range[0], 'custom', $format);
  $formatted2 = date_format_date($range[1], 'custom', $format);
  if ($formatted != $formatted2) {
    return $formatted .'-'. $formatted2;
  }
  else {
    return $formatted;
  }
}

/**
 * Views values vary depending on the op:
 * $op = 'summary'
 *   $argtype = name of the argument
 *   $query = views object
 *   $arg = argument type
 */
function _date_views_argument_summary(&$query, $argtype, $arg, $field_type) {
  $field_name = _date_views_arg_field_name($argtype);
  $field      = content_fields($field_name);
  $db_info    = content_database_info($field);
  $value      = substr($field_name, -1) == 2 ? 'value2' : 'value';
  $value      = $db_info['columns'][$value]['column'];
  $table      = 'node_data_'. $field['field_name'];
  $sql_field = "$table.$value";
  
  $date_handler = new date_sql_handler();
  $date_handler->construct($field_type);
  $date_handler->sql_field_name = $sql_field;
  $date_handler->granularity = $arg;
  date_views_set_timezone($date_handler, $field);
  
  $query->date_handler = $date_handler;
  $format = $date_handler->views_formats($date_handler->granularity, 'sql');
  $fieldinfo['field'] = $date_handler->sql_format($format, $date_handler->sql_field($sql_field));
  $fieldinfo['fieldname'] = 'range';
  
  $query->ensure_table($table);
  return $fieldinfo;
}

/**
 * Views values vary depending on the op:
 * $op = 'sort'
 *   $argtype = 'ASC' or 'DESC'
 *   $query = views object
 *   $arg = <empty>
 */
function _date_views_argument_sort(&$query, $argtype, $arg, $field_type) {
  // Add no sorting or we will get a groupby for the sort field
  // which will keep the summary totals from working right.
  return;
}

/**
 * Views values vary depending on the op:
 * $op = 'link'
 *   $argtype = name of the argument
 *   $query = object with child values
 *   $arg = url
 */
function _date_views_argument_link(&$query, $argtype, $arg, $field_type) {
  if (empty($query->range)) {
    return t('N/A');
  }
  $date_handler = new date_sql_handler();
  $granularity = $date_handler->arg_granularity($query->range);
  $format = $date_handler->views_formats($granularity, 'display');
  $range = $date_handler->arg_range($query->range);
  $formatted = date_format_date($range[0], 'custom', $format);
  $formatted2 = date_format_date($range[1], 'custom', $format);
  if ($formatted != $formatted2) {
    return l($formatted .'-'. $formatted2, $arg .'/'. $query->range);
  }
  else {
    return l($formatted, $arg .'/'. $query->range);
  }
}

/**
 * Helper function to find the field name in the argtype.
 */
function _date_views_arg_field_name($argtype) {
  $name = explode(':', $argtype);
  $tofield_name = trim($name[1]);
  return drupal_substr($tofield_name, 0, 3) == 'to|' ? drupal_substr($tofield_name, 3) : $tofield_name;
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
 *  Implementation of hook_views_query().
 * 
 *  Used to make sure Date Browser view defaults to current date if no date selected
 */
function _date_views_query_alter(&$query, &$view) {
  require_once('./'. drupal_get_path('module', 'date_api') .'/date_api_sql.inc');
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
    date_views_set_timezone($date_handler, $field);
    $view->date_handler = $date_handler;

    // Add a combo FROM|TO date field to the query
    $value  = $db_info['columns']['value']['column'];
    $value2 = !empty($db_info['columns']['value2']['column']) ? $db_info['columns']['value2']['column'] : $db_info['columns']['value']['column'];
    $value1 = $table .'.'. $value;
    $value2 = date_sql_coalesce(array($table .'.'. $value2, $table .'.'. $value));
    $combo  = date_sql_concat(array($value1, "'|'", $value2)) .' AS date_combo ';
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
      $arg = date_views_browser_period_arg(NULL, $view->argument[0]['options']);
      if ($range  = $view->date_handler->arg_range($arg)) {
        $query->ensure_table($table);
        $query->add_field('nid', 'node');
        $query->add_field($value, $table);
        $sql_field = "$table.$value";
        $query->add_where($date_handler->sql_where_date('DATE', $sql_field, ">=", date_format($range[0], DATE_FORMAT_DATETIME)));
        $query->add_where($date_handler->sql_where_date('DATE', $sql_field, "<=", date_format($range[1], DATE_FORMAT_DATETIME)));
      }
    }
    else {
      $range  = $view->date_handler->arg_range($arg);
    }
    $view->min_date = $range[0];
    $view->max_date = $range[1];
    if (empty($view->date_fields)) {
      $view->date_fields = array();
    }
    $view->date_fields[] = $field_name;
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
      $argument_list = implode("','", array_keys($arguments));
      if (!$argument_list) {
        return array();
      }
      $argument_list = "'". $argument_list ."'";
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
 *  Format an argument for the date range
 */
function date_views_browser_period_arg($arg = NULL, $period = 'month') {
  $date_handler = new date_sql_handler();
  $range = $date_handler->arg_range($arg);
  $date = $range[0];
  $format = $date_handler->views_formats($period, 'sql');
  $period_date = $period == 'week' ? date_format($date, 'Y-\W') . date_week(date_format($date, 'Y-m-d')) : date_format($date, $format);
  return  $period_date . date_views_browser_period($period);
}

/**
 *  Return label for current date range
 */
function date_views_browser_period_label($arg = NULL, $period = 'month') {
  $date_handler = new date_sql_handler();
  $range = $date_handler->arg_range($arg);
  return theme('date_views_browser_period_label', $period, $range[0]);
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
  if (empty($arg)) {
    $arg = date_views_browser_period_arg(NULL, $view->argument[0]['options']);
  }
  
  // Create a date object for the beginning of the current range.
  $range = $view->date_handler->arg_range($arg);
  $format = $view->date_handler->views_formats($period, 'sql');
  $date = drupal_clone($range[0]);
  // Move backwards 1 period and create a link to that date.
  date_modify($date, '-1'. $period);
  $prev = $period == 'week' ? date_format($date, 'Y-\W') . date_week(date_format($date, 'Y-m-d')) : date_format($date, $format);
  $prev = $view->url .'/'. $prev . date_views_browser_period($period);
  // Move forward 2 periods (back to the current period and to the next period)
  // and create a link to that date.
  date_modify($date, '+2 '. $period);
  $next = $period == 'week' ? date_format($date, 'Y-\W') . date_week(date_format($date, 'Y-m-d')) : date_format($date, $format);
  $next = $view->url .'/'. $next . date_views_browser_period($period);
  $label = date_views_browser_period_label($arg, $period);
  return theme('date_views_browser_navigation', $label, $period, $prev, $next, $view);
}

/**
 * Central function for setting up the right timezone values
 * in the SQL date handler.
 * 
 * The date handler will use this information to decide if the
 * database value needs a timezone conversion.
 * 
 * In Views, we will always be comparing to a local date value,
 * so the goal is to convert the database value to the right
 * value to compare to the local value.
 */
function date_views_set_timezone(&$date_handler, $field) {
  $tz_handling  = $field['tz_handling'];
  switch ($tz_handling) {
    case 'date' :
      $date_handler->db_timezone = 'UTC';
      $date_handler->local_timezone_field = $field['timezone_field'];
      $date_handler->offset_field = $field['offset_field'];
      break;
    case 'none':
      $date_handler->db_timezone = date_default_timezone_name();
      $date_handler->local_timezone = date_default_timezone_name();
      break;
    case 'utc':
      $date_handler->db_timezone = 'UTC';
      $date_handler->local_timezone = 'UTC';
      break;
    default :
      $date_handler->db_timezone = 'UTC';
      $date_handler->local_timezone = date_default_timezone_name();
      break;
  }
}