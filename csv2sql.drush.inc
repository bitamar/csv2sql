<?php

/**
 * @file
 * Convert CSV to SQL and import to the Drupal instance.
 */


/**
 * Implements hook_drush_command().
 */
function csv2sql_drush_command() {
  $items = array();

  $items['csv2sql'] = array(
    'description' => 'Convert CSV to SQL and import to the Drupal instance.',
    'examples' => array(
      'drush csv2sql /path/to/someFile.csv' => 'Converts the someFile.csv to an SQL table.',
    ),
    'arguments' => array(
      'path' => 'The path to the CSV file.',
    ),
    'required-arguments' => TRUE,
    'options' => array(
      'prefix' => 'the prefix of the table. Defaults to "_raw".'
    ),
  );

  return $items;
}

/**
 * Implements drush_hook_command().
 *
 * Command callback for csv2sql.
 */
function drush_csv2sql($csv_path) {
  $prefix = drush_get_option('prefix', '_raw');

  if (!file_exists($csv_path)) {
    drush_print('Given file does not exist.');
    return;
  }

  $path_info = pathinfo($csv_path);
  $table_name = $prefix . '_' . $path_info['filename'];

  if (($handle = fopen($csv_path, 'r')) !== FALSE) {
    $first_row = TRUE;
    while (($data = fgetcsv($handle, 0, ',')) !== FALSE) {
      if ($first_row) {
        $first_row = FALSE;

        // Create the table.
        $headers = csv2sql_create_db($table_name, $data);
        continue;
      }

      // Insert rows.
      $row = array();
      foreach ($data as $delta => $value) {
        $header_col = $headers[$delta];
        $row[$header_col] = $value;
      }
      csv2sql_insert_row_to_table($table_name, $row);
    }
    fclose($handle);
  }
}

/**
 * Create a DB.
 *
 * @param $table_name
 *   The table name.
 * @param array $header
 *   Array with the column names and definition that was extracted from the CSV.
 * @param bool $drop_existing
 *   Determines if an existing table should be dropped. Defaults to TRUE.
 *
 * @return array
 *   Array with the column names.
 */
function csv2sql_create_db($table_name, $header = array(), $drop_existing = TRUE) {
  // Add a serial key as the first column.
  $fields_info = array(
    'id' => array(
      'type' => 'serial',
      'not null' => TRUE,
      'description' => 'Primary Key: Numeric ID.',
    ),
  );

  $first_col = TRUE;

  // Get the column properties.
  foreach ($header as $col) {
    $header_info = explode('|', $col);
    $col_info = array();

    // Allow passing complex headers,
    // e.g. "Amount|type:int|length:11|not null:false"
    // Which will translate to a column "amount" of type int(11) and NULL value
    // is allowed.
    if (!empty($header_info[1])) {
      foreach (explode('|', $col_info) as $schemas) {
        foreach ($schemas as $schema) {
          foreach (explode(':', $schema) as $key => $value) {
            $col_info[$key] = $value;
          }
        }
      }
    }

    // Add default values.
    $col_info += array(
      'description' => '',
      'type' => 'varchar',
      'length' => 255,
      'not null' => TRUE,
      'default' => '',
    );

    if ($first_col) {
      // Set as primary key.
      $col_info += array();
      $first_col = FALSE;
    }

    $col_name = csv2sql_get_column_name($header_info[0]);
    $fields_info[$col_name] = $col_info;
  }

  if ($drop_existing) {
    // Drop existing table.
    db_drop_table($table_name);
  }

  $table_schema = array(
    'fields' => $fields_info,
    'primary key' => array('id'),
  );

  db_create_table($table_name, $table_schema);

  $headers = array_keys($fields_info);

  // Remove the ID key.
  unset($headers[0]);

  return array_values($headers);
}


/**
 * Insert a single row to the table.
 *
 * @param $table_name
 *   The table name.
 * @param $row
 *   The row to insert.
 *
 * @return bool
 *   TRUE if the insert operation was successful.
 */
function csv2sql_insert_row_to_table($table_name, $row) {
  return db_insert($table_name)
    ->fields($row)
    ->execute();
}

/**
 * Get a machine readable name fromthe column name.
 *
 * @param $col_name
 *   The column name.
 *
 * @return string
 *   The trimmed, lower-cased and underscored name of the column.
 */
function csv2sql_get_column_name($col_name) {
  return trim(strtolower(str_replace(array('-', ' '), '_', $col_name)));
}

