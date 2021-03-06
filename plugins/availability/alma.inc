<?php


/**
 * @file
 * Ding availability plugin.
 */

$plugin = array(
  'description' => t('Alma availability plugin'),
  'version' => 1,
);

/**
 */
function alma_availability_items($provider_ids) {
  $ids = join(',', $provider_ids);
  $result = alma_client_invoke('get_availability', $ids);
  if (!$result) {
    $result = array();
  }

  return $result;
}

/**
 */
function alma_availability_details($provider_ids) {
  $ids = join(',', $provider_ids);

  $holding_parts = array('branch', 'department', 'location', 'sublocation', 'collection');
  $details = alma_client_invoke('catalogue_record_detail', $ids);

  $org = alma_get_organisation();
  $result = array();
  if ($details && isset($details['records'])) {
    foreach ($details['records'] as $alma_id => $record) {
      $holding = array(
        'local_id' => $alma_id, // Sure hope this holds.
        'available' => $record['available_count'] > 0,
        'reservable' => $record['show_reservation_button'],
        'show_reservation_button' => $record['show_reservation_button'],
        'reserved_count' => (int) $record['reservation_count'],
        'deferred_period' => FALSE,
        'holdings' => array(),
      );
      $total = $total_reservable = 0;

      foreach ($record['holdings'] as $branch_holding) {
        if (in_array($branch_holding['collection_id'], array('karens', 'karens-'))) {
          $holding['deferred_period'] = TRUE;
        }

        $parts = array();
        $total += (int) $branch_holding['total_count'];
        // Reservable is total items minus reference (which cannot be
        // loaned).
        $reservable = (int) $branch_holding['total_count'] - (int) $branch_holding['reference_count'];
        $total_reservable += $reservable;
        foreach ($holding_parts as $part) {
          if (!empty($branch_holding[$part . '_id'])) {
            $parts[] = $org[$part][$branch_holding[$part . '_id']];
          }
        }

        if (!empty($branch_holding['shelf_mark'])) {
          // Shelf mark might have leading >, strip any and replace the rest
          // with the proper arrow.
          $parts[] = strtr(trim($branch_holding['shelf_mark'], " >\n\t"), array('>' => '→'));
        }

        $parts = array_filter($parts);

        if ($parts && $branch_holding['total_count'] > $branch_holding['checked_out_count']) {
          $holding['holdings'][] = join(' → ', $parts);
        }
      }

      $holding['reservable_count'] = $total_reservable;
      $holding['total_count'] = $total;
      $result[$alma_id] = $holding;
    }
  }

  return $result;
}

/**
 *
 */
function alma_availability_periodical($provider_ids) {
  $ids = join(',', $provider_ids);

  $holding_parts = array('branch', 'department', 'location', 'sublocation', 'collection');
  $details = alma_client_invoke('catalogue_record_detail', $ids);
  $reservable_statuses = array('availableForLoan', 'checkedOut', 'periodical', 'inAcquisition', 'ordered', 'openLoan');

  $result = array();
  if ($details && isset($details['records'])) {
    foreach ($details['records'] as $alma_id => $record) {
      $holding = array(
        'local_id' => $alma_id, // Sure hope this holds.
        'available' => $record['available_count'] > 0,
        'reservable' => $record['show_reservation_button'],
        'show_reservation_button' => $record['show_reservation_button'],
        'reserved_count' => (int) $record['reservation_count'],
        'issues' => array(),
      );

      foreach ($record['holdings'] as $year => $issues) {
        if (!isset($holding['holdings'][$year])) {
          $holding['issues'][$year] = array();
        }

        foreach ($issues as $issue_title => $issue_holdings) {
          $issue = array(
            'local_id' => $issue_holdings[0]['reservable'], // Yeah, odd, but...
            'reservable' => FALSE,
          );
          $issue['reservable'] = FALSE;
          foreach ($issue_holdings as $issue_holding) {
            if (in_array($issue_holding['status'], $reservable_statuses)) {
              $issue['reservable'] = TRUE;
              break;
            }
          }
          $holding['issues'][$year][$issue_title] = $issue;
        }
        // Sort issues in reverse chronological order.
        ksort($holding['issues']);
        $holding['issues'] = array_reverse($holding['issues'], TRUE);
      }

      $result[$alma_id] = $holding;
    }
  }

  return $result;
}
