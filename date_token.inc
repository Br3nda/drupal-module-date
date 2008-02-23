<?php
//$Id: date_token.inc,v 1.1.2.1 2008/02/23 13:13:34 karens Exp $
/**
 * @file
 * Token module integration.
 */

function _date_token_list($type = 'all') {
  if ($type == 'field' || $type == 'all') {
    $tokens = array();

    $tokens['date']['value']          = t("The raw date value.");
    $tokens['date']['view']           = t("The formatted date.");
    $tokens['date']['timestamp']      = t("The raw date timestamp.");
    $tokens['date']['yyyy']           = t("Date year (four digit)");
    $tokens['date']['yy']             = t("Date year (two digit)");
    $tokens['date']['month']          = t("Date month (full word)");
    $tokens['date']['mon']            = t("Date month (abbreviated)");
    $tokens['date']['mm']             = t("Date month (two digit, zero padded)");
    $tokens['date']['m']              = t("Date month (one or two digit)");
    $tokens['date']['ww']             = t("Date week (two digit)");
    $tokens['date']['date']           = t("Date date (day of month)");
    $tokens['date']['day']            = t("Date day (full word)");
    $tokens['date']['ddd']            = t("Date day (abbreviation)");
    $tokens['date']['dd']             = t("Date day (two digit, zero-padded)");
    $tokens['date']['d']              = t("Date day (one or two digit)");

    $tokens['date']['to-????']        = t("If the field has a to-date defined, the same tokens exist in the form: [to-????], where ???? is the normal token.");

    return $tokens;
  }
}

function _date_token_values($type, $object = NULL, $options = array()) {
  if ($type == 'field') {
    include_once(drupal_get_path('module', 'date') .'/date.inc');
    $item = $object[0];

    $item['value'] = trim($item['value']);
    $item_format = (is_numeric($item['value']) ? DATE_UNIX : DATE_ISO);
    $date = date_make_date($item['value'], 'none', 'local', $item_format);

    $tokens['value']          = $item['value'];
    $tokens['view']           = $item['view'];
    $tokens['timestamp']      = $date->local->timestamp;
    $tokens['yyyy']           = date_show_date($date, 'Y');
    $tokens['yy']             = date_show_date($date, 'y');
    $tokens['month']          = date_show_date($date, 'F');
    $tokens['mon']            = date_show_date($date, 'M');
    $tokens['mm']             = date_show_date($date, 'm');
    $tokens['m']              = date_show_date($date, 'n');
    $tokens['ww']             = date_show_date($date, 'W');
    $tokens['date']           = date_show_date($date, 'N');
    $tokens['day']            = date_show_date($date, 'l');
    $tokens['ddd']            = date_show_date($date, 'D');
    $tokens['dd']             = date_show_date($date, 'd');
    $tokens['d']              = date_show_date($date, 'j');

    if (!empty($item['value2'])) {

      $item['value2'] = trim($item['value2']);
      $item_format = (is_numeric($item['value2']) ? DATE_UNIX : DATE_ISO);
      $date = date_make_date($item['value2'], 'none', 'local', $item_format);

      $tokens['to-value']          = $item['value2'];
      $tokens['to-view']           = $item['view2'];
      $tokens['to-timestamp']      = $date->local->timestamp;
      $tokens['to-yyyy']           = date_show_date($date, 'Y');
      $tokens['to-yy']             = date_show_date($date, 'y');
      $tokens['to-month']          = date_show_date($date, 'F');
      $tokens['to-mon']            = date_show_date($date, 'M');
      $tokens['to-mm']             = date_show_date($date, 'm');
      $tokens['to-m']              = date_show_date($date, 'n');
      $tokens['to-ww']             = date_show_date($date, 'W');
      $tokens['to-date']           = date_show_date($date, 'N');
      $tokens['to-day']            = date_show_date($date, 'l');
      $tokens['to-ddd']            = date_show_date($date, 'D');
      $tokens['to-dd']             = date_show_date($date, 'd');
      $tokens['to-d']              = date_show_date($date, 'j');
    }
    return $tokens;
  }
}
