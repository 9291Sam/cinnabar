#!/bin/zsh

# ==============================================================================
#  Endpoint Discovery Script for Medical Device
#
#  Instructions:
#  1. Fill in your device's serial number, username, and password below.
#  2. Make the script executable: chmod +x your_script_name.sh
#  3. Run the script: ./your_script_name.sh
#
#  The script will test a list of potential endpoints and print the HTTP
#  status code for each. A '200' status code usually means success.
# ==============================================================================

# --- USER CONFIGURATION ---
# Replace with your specific details

# --- ENDPOINT GUESSES ---
# An array of potential endpoints to test.
# We are looking for endpoints that might provide real-time numeric or waveform data.
endpoints=(
  # --- TIER 1: PRIME CANDIDATES (Based on Documentation) ---
  # The PDF states "Snapshot Record...captures an interval of waveform data". This is the #1 lead.
  "Snapshot"
  "Snapshot/current"
  "Snapshot/latest"
  "Snapshot/now"
  "Snapshot/live"
  "Snapshot/current?now"
  "Numerics/current?now"    # Confirmed in docs, good for baseline
  "Trend/current?now"       # Confirmed in docs, good for baseline

  # --- TIER 2: STANDARD API NAMING CONVENTIONS ---
  # Common, top-level resource names for this type of data.
  "Waveform"
  "Waveforms"
  "Pleth"
  "Stream"
  "RealTime"
  "Live"
  "SpO2"
  "Vitals"
  "Data"

  # --- TIER 3: LOGICAL PERMUTATIONS & COMBINATIONS ---
  # Mixing standard names with common actions.
  "Waveform/current"
  "Waveform/latest"
  "Waveform/live"
  "Waveform/stream"
  "Waveform/Pleth"
  "Waveform/SpO2"
  "Waveform/now"
  "Waveform/current?now"
  "Waveforms/Pleth"
  "Waveforms/live"

  "Stream/current"
  "Stream/latest"
  "Stream/Pleth"
  "Stream/SpO2"
  "Stream/Waveform"
  "Stream/Vitals"

  "Pleth/current"
  "Pleth/latest"
  "Pleth/live"
  "Pleth/stream"
  "Pleth/now"

  "RealTime/Pleth"
  "RealTime/Waveform"
  "RealTime/Vitals"

  "Live/Pleth"
  "Live/Waveform"
  "Live/Vitals"

  "SpO2/Waveform"
  "SpO2/Pleth"
  "SpO2/live"

  # --- TIER 4: DEEP & CREATIVE GUESSES ---
  # Less common patterns, including API versioning, different parameter styles, and compound names.
  "api/v1/Waveform"
  "api/v1/Stream"
  "api/v1/Pleth"
  "api/v2/Waveform"

  "data/waveform"
  "data/pleth"
  "data/stream"
  "data/live"

  "vitals/current"
  "vitals/live"
  "vitals/Pleth"

#   "Case/current/Waveform" # Using the documented /Case/current structure
#   "Case/current/Pleth"
#   "Case/current/waveforms"
#   "Case/current/Vitals"

  "Plethysmograph"
  "Plethysmograph/live"

  "getWaveform"
  "getPleth"
  "getRealTimeData"

  # Testing query parameters
  "data?source=pleth"
  "stream?id=pleth"
  "waveform?type=pleth"
  "snapshot?type=waveform"

  # --- TIER 5: CASE SENSITIVITY VARIATIONS ---
  # The documentation explicitly states endpoints are case-sensitive.
  "waveform"
  "waveforms"
  "pleth"
  "stream"
  "realtime"
  "live"
  "vitals"
  "SPO2"
  "PLETH"
  "WAVEFORM"
)
685586BABEDA11CAAB8181E617001702
685586BBBF5311CAAB8381E617001702
685586BBBFCD11CAAB8581E617001702
685586BBC04211CAAB8981E617001702
685586BBC0BB11CAAB8B81E617001702
685586BBC13311CAAB8D81E617001702
685586BBC1AB11CAAB8F81E617001702
685586BBC29A11CAAB9381E617001702
685586BBC22311CAAB9181E617001702


# --- SCRIPT LOGIC ---
echo "Starting endpoint discovery for device at ${SERIAL_NUM}.x-series.device.zoll.local..."
echo "-----------------------------------------------------"

for endpoint in "${endpoints[@]}"; do
  # Construct the full URL for the current endpoint guess
  URL="https://ap17g001702.x-series.device.zoll.local/${endpoint}"

  echo -n "Testing endpoint: ${URL} ... "

  # Use curl to make the request.
  # -s: Silent mode (don't show progress meter)
  # -k: Allow insecure server connections (for self-signed certs)
  # -o /dev/null: Discard the output body (we only want the status code for now)
  # -w '%{http_code}': Write out the HTTP status code
  # --connect-timeout 5: Max time to wait for a connection
  HTTP_STATUS=$(curl -s -k -o /dev/null -w "%{http_code}" --connect-timeout 5 \
    -H "Accept:application/json" \
    -H "Client-Id:12345" \
    -u "zolldata:Hello,1'mFromZOLL" \
    "${URL}")

  echo "Status: ${HTTP_STATUS}"

  # Once you find a working endpoint (e.g., status 200), you can comment out
  # the line above and uncomment the one below to see the actual data.
  #
  # curl -s -k -H "Accept:application/json" -H "Client-Id:12345" -u "${USERNAME}:${PASSWORD}" "${URL}"
  # echo "" # for new line

  # Wait for 2 seconds before the next request to be safe
#   sleep 2
done

echo "-----------------------------------------------------"
echo "Discovery finished."
