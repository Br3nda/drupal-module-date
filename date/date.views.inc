<?php
// $Id: date.views.inc,v 1.6 2010/03/01 00:40:08 karens Exp $
/**
 * @file
 * Date Views code.
 */
/**
 * Implementation of hook_views_handlers().
 */
function date_views_handlers() {
  return array(
    'info' => array(
      'path' => drupal_get_path('module', 'date'),
    ),
    // A date-specific handler for grouping multiple values.
    'handlers' => array(
      'date_handler_field_multiple' => array(
        'parent' => 'content_handler_field_multiple',
      ),
     ),
   );
}
