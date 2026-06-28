#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<USAGE
Usage:
  $0 --ped PED_JSON [--workdir WORKDIR] [--tag TAG]

Purpose:
  End-to-end validation of the RecHit/level0 calibration no-intermediate-JSON workflow.

Workflow:
  1. Build a reference level0 calibration JSON from the input pedestal JSON.
  2. Run PrepareLevel0CalibParams.py with --sqlite-output and --no-json-output.
  3. Write the HGCalRecHitCalibrationConditions payload to SQLite through cmsRun.
  4. Read the SQLite payload back through PoolDBESSource/EventSetup.
  5. Compare it against the reference level0 JSON.

Required:
  --ped       Input pedestal JSON passed to PrepareLevel0CalibParams.py

Optional:
  --workdir   Working directory for outputs. Default: rechitcalib_nojson_test
  --tag       SQLite CondDB tag. Default: HGCalRecHitCalibration_nojson_test

Example:
  $0 --ped /path/to/pedestals.json
USAGE
}

PED_JSON=""
WORKDIR="rechitcalib_nojson_test"
TAG="HGCalRecHitCalibration_nojson_test"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ped)
      PED_JSON="$2"
      shift 2
      ;;
    --workdir)
      WORKDIR="$2"
      shift 2
      ;;
    --tag)
      TAG="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1"
      usage
      exit 1
      ;;
  esac
done

if [[ -z "${PED_JSON}" ]]; then
  echo "ERROR: --ped PED_JSON is required"
  usage
  exit 1
fi

if [[ ! -f "${PED_JSON}" ]]; then
  echo "ERROR: pedestal JSON does not exist: ${PED_JSON}"
  exit 1
fi

if [[ -z "${CMSSW_BASE:-}" ]]; then
  echo "ERROR: CMSSW environment is not set. Run cmsenv first."
  exit 1
fi

SCRIPT="RecoLocalCalo/HGCalRecAlgos/scripts/PrepareLevel0CalibParams.py"
WRITE_CFG="RecoLocalCalo/HGCalRecAlgos/test/HGCalRecHitCalibrationCondDB_cfg.py"
READBACK_CFG="RecoLocalCalo/HGCalRecAlgos/test/HGCalRecHitCalibrationDBReadback_cfg.py"

if [[ ! -f "${SCRIPT}" ]]; then
  echo "ERROR: cannot find ${SCRIPT}"
  exit 1
fi

if [[ ! -f "${WRITE_CFG}" ]]; then
  echo "ERROR: cannot find ${WRITE_CFG}"
  exit 1
fi

if [[ ! -f "${READBACK_CFG}" ]]; then
  echo "ERROR: cannot find ${READBACK_CFG}"
  exit 1
fi

mkdir -p "${WORKDIR}"

REF_JSON="${WORKDIR}/level0_calib_reference.json"
SQLITE_FILE="${WORKDIR}/hgcal_rechit_calibration_nojson_test.db"

rm -f "${SQLITE_FILE}" "${REF_JSON}"

echo "============================================================"
echo "[1/3] Building reference level0 JSON for closure comparison"
echo "============================================================"
python3 "${SCRIPT}" \
  --ped "${PED_JSON}" \
  --output "${REF_JSON}"

if [[ ! -f "${REF_JSON}" ]]; then
  echo "ERROR: reference JSON was not produced: ${REF_JSON}"
  exit 1
fi

echo
echo "Reference JSON:"
echo "  ${REF_JSON}"

echo
echo "============================================================"
echo "[2/3] Writing SQLite through the no-intermediate-JSON path"
echo "============================================================"
python3 "${SCRIPT}" \
  --ped "${PED_JSON}" \
  --sqlite-output "${SQLITE_FILE}" \
  --sqlite-tag "${TAG}" \
  --sqlite-record HGCalRecHitCalibrationRcd \
  --sqlite-since-run 1 \
  --sqlite-cfg "${WRITE_CFG}" \
  --no-json-output

if [[ ! -f "${SQLITE_FILE}" ]]; then
  echo "ERROR: SQLite file was not produced: ${SQLITE_FILE}"
  exit 1
fi

echo
echo "SQLite file:"
echo "  ${SQLITE_FILE}"

echo
echo "============================================================"
echo "[3/3] Running SQLite readback closure test"
echo "============================================================"
cmsRun "${READBACK_CFG}" \
  sqliteFile="${SQLITE_FILE}" \
  dbTag="${TAG}" \
  record=HGCalRecHitCalibrationRcd \
  refJson="${REF_JSON}"

echo
echo "============================================================"
echo "No-intermediate-JSON RecHitCalib workflow validation PASSED"
echo "============================================================"
echo "Input pedestal JSON : ${PED_JSON}"
echo "Reference level0 JSON: ${REF_JSON}"
echo "SQLite file         : ${SQLITE_FILE}"
echo "Tag                 : ${TAG}"
