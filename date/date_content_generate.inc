<?php
/**
 * Implementation of Devel module's hook_content_generate().
 *
 * Included only when needed.
 */
function _date_content_generate(&$node, $field) {
  $node_field = array();
  if (isset($field['widget']['year_range'])) {
    $split = explode(':', $field['widget']['year_range']);
    $back = str_replace('-', '', $split[0]);
    $forward = str_replace('+', '', $split[1]);
  }
  else {
    $back = 2;
    $forward = 2;
  }
  // Pick a random year within the time range,
  // and a random second within that year.
	$year = date_format(date_now(), 'Y') - $back + rand(0, ($forward + $back));
	$start = date_make_date($year .'-01-01 00:00:00', 'UTC');
	$leap = date_format($start, 'L');
	$max_days = $leap ? 366 : 365;
	$seconds = mt_rand(0, ($max_days * 86400));
	date_modify($start, "+$seconds seconds");

	// Modify To date by 1 hour to 3 days, shorter for repeating dates
	// longer for others.
	$start2 = drupal_clone($start);
	$max = !empty($field['repeat']) ? 720 : 4320;
  date_modify($start2, '+'. mt_rand(60, $max) .' minutes');  
    
  if ($field['tz_handling'] == 'date') {
    // Choose a random timezone.
    // Not all keys exist, so we have to check.
    $timezones = array_keys(date_timezone_names(TRUE));
    $key = mt_rand(0, count($timezones) - 1);
    if (!array_key_exists($key, $timezones)) {
      $timezone = date_default_timezone_name();
    }
    else {
      $timezone = $timezones[$key];
    }
    $timezone = $timezones[$key];
  }
    
  switch ($field['type']) {
    case 'date':
      $format = DATE_FORMAT_ISO;
      break;
    case 'datestamp':
      $format = DATE_FORMAT_UNIX;
      break;
    case 'datetime':
      $format = DATE_FORMAT_DATETIME;
      break;
  }
  $count = $field['multiple'] > 0 && $field['multiple'] < 99  ? rand(1, 6) : 1;
  for($i = 0; $i < $count; $i++) {
    $node_field[$i]['value'] = date_format($start, $format);
    if ($field['todate']) {
      $node_field[$i]['value2'] = date_format($start2, $format);
    }
    if ($field['tz_handling'] == 'date') {
      date_timezone_set($start, timezone_open($timezone));
      $node_field[$i]['timezone'] = $timezone;
      $node_field[$i]['offset'] = date_offset_get($start);
      if ($field['todate']) {
        date_timezone_set($start2, timezone_open($timezone));
        $node_field[$i]['offset2'] = date_offset_get($start2);     	 	
      }
    }
  }
  if (!empty($field['repeat'])) {
    include_once('./'. drupal_get_path('module', 'date_repeat') .'/date_repeat_calc.inc');
    include_once('./'. drupal_get_path('module', 'date') .'/date_repeat.inc');
    include_once('./'. drupal_get_path('module', 'date_api') .'/date_api_ical.inc');
  
    // Create a repeating date.
    $duration = date_difference($start, $start2);
    $form_values = array();
    $which = rand(0, 2);
    switch ($which) {
      case 0:
        $dows = array_keys(date_repeat_dow_options());
        $day = date_content_generate_key($dows);
        $dow = $dows[$day];
        $options = array('MONTHLY', 'DAILY', 'WEEKLY');
        $freq = date_content_generate_key($options);
        $freq = $options[$freq];
        $form_values['FREQ'] = $freq;
        $form_values['BYDAY'] = array($dow);
        break;
      case 1:
        $mo = rand(1, 28);
        $options = array('YEARLY', 'MONTHLY');
        $freq = date_content_generate_key($options);
        $freq = $options[$freq];
        $form_values['FREQ'] = $freq;
        $form_values['BYMONTHDAY'] = array($mo);
        break;
      case 2:
        $mo = rand(1, 12);
        $options = array('YEARLY', 'MONTHLY');
        $freq = date_content_generate_key($options);
        $freq = $options[$freq];
        $form_values['FREQ'] = $freq;
        $form_values['BYMONTH'] = array($mo);
        break;
    }
    
    $intervals = array_keys(INTERVAL_options());
    unset($intervals[0]);
    $interval = $intervals[rand(1, 3)];
    $form_values['INTERVAL'] = $interval;
    
    $period = str_replace('LY', '', $freq);
    date_modify($start2, '+'. (mt_rand(2, 5) * $interval) .' '. strtolower($period));
    $until = date_format($start2, 'Y-m-d H:i:s');
    $form_values['UNTIL'] = array('datetime' => $until, 'tz' => 'UTC');
    
    $rrule = date_api_ical_build_rrule($form_values);
    $values = date_repeat_build_dates($rrule, $form_values, $field, $node_field);
    
    $start = $node_field;
    $node_field = array(0 => $start);
    $node_field[0]['rrule'] = $rrule;
    $node_field += $values;
  }
  return $node_field;
}

function date_content_generate_key($array) {
  $keys = array_keys($array);
  $min = array_shift($keys);
  $max = array_pop($keys);
  return mt_rand($min, $max);
}