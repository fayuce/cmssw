#!/usr/bin/env python3
import os, re
import json
import datetime
import subprocess
from argparse import ArgumentParser, RawTextHelpFormatter
import numpy as np  # type: ignore
from typing import Union
from multiprocessing import Pool
try:
    import HGCalCommissioning.LocalCalibration.JSONEncoder as JS  # type: ignore
except ModuleNotFoundError:
    JS = None

try:
    from HGCalCommissioning.LocalCalibration.HGCalDBHelper import HGCalCalibrationsDBHelper  # type: ignore
except ModuleNotFoundError:
    HGCalCalibrationsDBHelper = None


def getChannelsFrom(typecode: str) -> int:
    """converts a typecode to number of channels
    if you want to recompute this open the cell maps with pandas
    df.groupby('Typecode')['ROC'].agg('count').to_dict()
    """
    typecode_to_ch = {
        "MH-B": 296,
        "MH-F": 444,
        "MH-L": 148,
        "MH-R": 148,
        "MH-T": 185,
        "ML-5": 222,
        "ML-B": 111,
        "ML-F": 222,
        "ML-L": 111,
        "ML-R": 111,
        "ML-T": 111,
        "TH-L34": 216,
        "TH-L35": 228,
        "TH-L36": 252,
        "TH-L37": 288,
        "TL-L38": 217,
        "TL-L39": 234,
        "TL-L40": 316,
        "TL-L41": 324,
        "TL-L42": 324,
        "TL-L43": 324,
        "TL-L44": 377,
        "TL-L45": 377,
        "TL-L46": 350,
        "TL-L47": 350,
        "TL-L44S1-TB2025": 185,
    }

    # if it is found use it
    if typecode in typecode_to_ch:
        return typecode_to_ch[typecode]

    # otherwise use nearest match
    for k, nch in typecode_to_ch.items():
        if typecode.find(k) != 0:
            continue
        return nch

    print(f"Warning : no match for {typecode}, returning max")
    return 444


def getCalibTemplate(dim: Union[str, int]):
    """builds the default template for the level-0 calibration json file
    dim is the number of channels or, if string is converted to the number of channels
    """

    if type(dim) == int:
        nch = dim
    else:
        nch = getChannelsFrom(dim)  # type: ignore

    z = np.zeros(nch).tolist()
    o = np.ones(nch).tolist()

    calib_templ_dict = {
        "Channel": [i for i in range(nch)],
        "ADC_ped": z,
        "Noise": z,
        "CM_ped": z,
        "CM_slope": z,
        "BXm1_slope": z,
        "BXm1_ped": z,
        "TOTtoADC": o,  # TB'25 [x * 9.085 for x in o],
        "TOT_ped": z,
        "TOT_lin": z,
        "TOT_P0": z,
        "TOT_P1": z,
        "TOT_P2": z,
        "TOA_CTDC": np.zeros((nch, 32)).tolist(),
        "TOA_FTDC": np.zeros((nch, 8)).tolist(),
        "TOA_TW": np.zeros((nch, 3)).tolist(),
        "MIPS_scale": o,  # TB'25 [x / 13.265 for x in o],
        "Valid": o,
    }

    return calib_templ_dict



def normalizeCalibrationInput(obj: dict) -> dict:
    """Normalize single-module calibration JSON files to the typecode-indexed format
    expected by buildLevel0CalibParams.

    Supported input:
      {
        "typeCode": "...",
        "reference": "...",
        "timestamp": "...",
        "payload": {...}
      }

    Output:
      {
        "<typeCode>": {...}
      }
    """
    if isinstance(obj, dict) and "payload" in obj and isinstance(obj["payload"], dict):
        typecode = obj.get("typeCode", obj.get("typecode", None))
        if typecode is not None:
            return {str(typecode): obj["payload"]}
    return obj


def buildLevel0CalibParams(args) -> tuple[str, dict]:

    typecode, input_json = args

    # load data to merge
    data: dict = {}
    for k, f in input_json.items():
        if type(f) == str:
            with open(f) as jsonf:
                loaded = normalizeCalibrationInput(json.load(jsonf))
                data[k] = loaded.get(typecode, {})
        else:
            loaded = normalizeCalibrationInput(f)
            data[k] = loaded.get(typecode, {})

    # build the calibration dict
    typecode = typecode.replace("_", "-")
    level0_calib: dict = getCalibTemplate(typecode)
    ped_calib = data.get("ped", {})
    if "Channel" in ped_calib:
        nch = len(ped_calib["Channel"])
        level0_calib = getCalibTemplate(nch)
        level0_calib["Channel"] = ped_calib["Channel"].copy()
        level0_calib["Valid"] = ped_calib["Valid"].copy()
        level0_calib["ADC_ped"] = ped_calib["adc_ped"].copy()
        level0_calib["Noise"] = ped_calib["adc_rms"].copy()
        level0_calib["CM_ped"] = ped_calib[f"cm2_ped"].copy()
        level0_calib["CM_slope"] = ped_calib[f"cm2_slope"].copy()
    # else:
    #  print(f'[Warning] Unavailable pedestal calibrations for {typecode}, assigned default based on typecode')

    # add the calpulse results also
    # as TOT will be linearized as function of ADC we apply the following transformation
    # q = k*(ADC-p)
    # linear: q = ktot*(TOT-ptot) <=> ADC = (ktot/k)*(TOT-ptot)+p
    # non-linear: q = p2*TOT**2 + p1*TOT + p0 <=> ADC = (p2/ktot)*TOT**2 + (p1/ktot)*TOT + (p0/ktot)
    calpulse_calib = data.get("calpulse", {})
    if len(calpulse_calib) > 0:
        mdata_np = dict([(k, np.array(v)) for k, v in calpulse_calib.items()])
        k = mdata_np["adc2fC"]
        p = mdata_np["adc_ped"]
        x0 = mdata_np["tot0"]
        ktot = mdata_np["tot2fC"]
        ptot = mdata_np["tot_ped"]
        b = mdata_np["totlin"]
        a = (ktot - b) / (2.0 * x0)
        c = ktot * (x0 - ptot) - 0.5 * (ktot + b) * x0

        level0_calib["TOTtoADC"] = (ktot / k).tolist()
        level0_calib["TOT_ped"] = (ptot - p * k / ktot).tolist()
        level0_calib["TOT_lin"] = x0.tolist()
        level0_calib["TOT_P2"] = (a / k).tolist()
        level0_calib["TOT_P1"] = (b / k).tolist()
        level0_calib["TOT_P0"] = (c / k + p).tolist()

    # add the mip scale
    mip_calib = data.get("mip", {})
    if "Channel" in mip_calib:
        mip_scale = np.array(mip_calib["mip"])
        mean_mip_scale = np.ones_like(mip_scale) * mip_scale.mean()
        level0_calib["MIPS_scale"] = np.where(
            mip_scale > 0, 1.0 / mip_scale, mean_mip_scale
        ).tolist()

    return (typecode, level0_calib)


def main():

    parser = ArgumentParser(
        description="""
    [Prepare the level 0 calibration json]
    The json can be prepared using the parameters described below. 
    To check carefully the distributions you can use the calibrations viewer app
    Some back of the envelope values to keep in mind

    | Quantity  | order of magnitude             | example                                  |
    | ========= | ============================== | ======================================== |
    | ADC_ped   | 100-200                        | 150                                      |
    | --------- | ------------------------------ | ---------------------------------------- |
    | ADC2fC    | fsc / (1024-pedestal)          | 0.19 for fsc = 160 fC and ADC_ped=150    |
    | --------- | ------------------------------ | ---------------------------------------- |
    | TOTtoADC  | (10pC / 2^12) / ( fsc / 2^10 ) | 15                                       |
    | --------- | ------------------------------ | ---------------------------------------- |  
    | MIP_scale | q_MIP / ADC2fC                 | for {120,200,300} microns                |
    |           |                                | q_MIP = {1.2, 1.9 3.5} fC                |
    |           |                                | for ADC2fC=0.19 300 microns MIP_scale=18 |
    """,
        epilog="Good luck!",
        formatter_class=RawTextHelpFormatter,
    )
    parser.add_argument(
        "-o",
        "--output",
        default="level0_calib_params.json",
        help="output JSON file, default=%(default)r",
    )
    parser.add_argument(
        "-p", "--ped", default=None, help="Pedestal file default=%(default)r"
    )
    parser.add_argument(
        "-c", "--calpulse", default=None, help="Calpulse file default=%(default)r"
    )
    parser.add_argument(
        "-m", "--mip", default=None, help="MIP file default=%(default)r"
    )
    parser.add_argument(
        "-t",
        "--test",
        default="",
        help="CSV list of typecodes to test baseline",
    )
    parser.add_argument(
        "--nthreads", type=int, default=8, help="number of parallel jobs to spawn"
    )
    parser.add_argument(
        "--push-to-db", action="store_true", help="push resulting JSON to Conditions DB"
    )
    parser.add_argument(
        "--mysql-env", type=str, default=None, help="Path to MySQL environment file"
    )
    parser.add_argument(
        "--sqlite-output",
        type=str,
        default=None,
        help="Optional output SQLite CondDB file for RecHitCalib/level0 payload",
    )
    parser.add_argument(
        "--sqlite-tag",
        type=str,
        default="HGCalRecHitCalibration_level0",
        help="CondDB tag name to use when --sqlite-output is provided",
    )
    parser.add_argument(
        "--sqlite-record",
        type=str,
        default="HGCalRecHitCalibrationRcd",
        help="CondDB record name to use when --sqlite-output is provided",
    )
    parser.add_argument(
        "--sqlite-since-run",
        type=int,
        default=1,
        help="IOV since run to use when writing the SQLite CondDB payload",
    )
    parser.add_argument(
        "--sqlite-cfg",
        type=str,
        default="src/RecoLocalCalo/HGCalRecAlgos/test/HGCalRecHitCalibrationCondDB_cfg.py",
        help="cmsRun cfg used to write the SQLite CondDB payload",
    )
    parser.add_argument(
        "--no-json-output",
        action="store_true",
        help="Do not write the intermediate level0 JSON file when --sqlite-output is used",
    )
    parser.add_argument(
        "--run-esproducer-closure",
        action="store_true",
        help="After writing SQLite, run the ESProducer-based closure test using the in-memory JSON content",
    )
    parser.add_argument(
        "--esproducer-closure-cfg",
        type=str,
        default="src/RecoLocalCalo/HGCalRecAlgos/test/HGCalRecHitCalibrationESProducerClosure_cfg.py",
        help="cmsRun cfg used for the ESProducer-based SQLite closure test",
    )
    parser.add_argument(
        "--closure-tolerance",
        type=float,
        default=1e-5,
        help="Relative tolerance for the ESProducer-based closure comparison",
    )
    parser.add_argument(
        "--closure-modules",
        type=str,
        default="Geometry/HGCalMapping/data/ModuleMaps/modulelocator_Sep2024TBv2.txt",
        help="Module locator file passed to the ESProducer closure cfg",
    )
    args = parser.parse_args()

    # parse arguments and check how may are available
    input_json: dict = {}
    exp_keys: list = ["ped", "mip", "calpulse"]
    test_modules = [x for x in args.test.split(",") if len(x) > 0]
    if len(test_modules) > 0:
        level0_calib = dict([(k, getCalibTemplate(k)) for k in test_modules])
    else:

        for k in exp_keys:
            k_val = getattr(args, k)
            if k_val is None:
                continue
            input_json[k] = k_val

        if len(input_json) == 0:
            raise ValueError(f"Expect at least one of {exp_keys}")

        # load first json to get typecodes
        # build calib dicts in parallel and then merge
        first_input = next(iter(input_json))
        with open(input_json[first_input]) as jsonf:
            loaded = normalizeCalibrationInput(json.load(jsonf))
            tasks = [(typecode, input_json) for typecode in loaded.keys()]
        print(f"Launching {len(tasks)} tasks")
        with Pool(args.nthreads) as pool:
            results = pool.map(buildLevel0CalibParams, tasks)
            print(f"Collected {len(results)} calibration snippets")
            level0_calib = dict(
                (typecode, typecode_calib) for typecode, typecode_calib in results
            )

    if args.push_to_db and args.no_json_output:
        raise ValueError("--push-to-db requires a JSON output file; do not use --no-json-output with --push-to-db")

    if args.run_esproducer_closure and not args.sqlite_output:
        raise ValueError("--run-esproducer-closure requires --sqlite-output")

    # Temporary test aliases for closure without a modulelocator file.
    # The default mapping may expose generic module names such as MH-F1W / ML-F2W,
    # while the level0 payload contains serial-specific keys such as MH-F1W-CNT0137.
    # Add one representative generic alias so the ESProducer closure can exercise
    # SQLite -> PoolDBESSource -> EventSetup -> ESProducer -> SoA.
    # save final output
    if args.no_json_output:
        print("Skipping level0 JSON output file (--no-json-output)")
    else:
        print(f"Writing to {args.output}")
        if JS is not None:
            JS.saveAsJson(args.output, level0_calib)
        else:
            with open(args.output, "w") as jsonf:
                json.dump(level0_calib, jsonf, indent=2)

    # Serialize once in memory. This is not written to an intermediate JSON file.
    level0_calib_json = json.dumps(level0_calib)

    # ---- write SQLite CondDB file if requested ----
    if args.sqlite_output:
        cmd = [
            "cmsRun",
            args.sqlite_cfg,
            "jsonFile=-",
            f"sqliteFile={args.sqlite_output}",
            f"record={args.sqlite_record}",
            f"tag={args.sqlite_tag}",
            f"sinceRun={args.sqlite_since_run}",
            "writeToCondDB=True",
        ]
        print("Writing SQLite CondDB with cmsRun: " + " ".join(cmd))
        subprocess.run(cmd, input=level0_calib_json, text=True, check=True)

    # ---- run ESProducer-based SQLite closure if requested ----
    if args.run_esproducer_closure:
        cmd = [
            "cmsRun",
            args.esproducer_closure_cfg,
            f"sqliteFile={args.sqlite_output}",
            f"calibTag={args.sqlite_tag}",
            "referenceJson=-",
            f"tolerance={args.closure_tolerance}",
            f"modules={args.closure_modules}",
        ]
        print("Running ESProducer SQLite closure with cmsRun: " + " ".join(cmd))
        subprocess.run(cmd, input=level0_calib_json, text=True, check=True)

    # ---- push to DB if requested ----
    if args.push_to_db:
        if HGCalCalibrationsDBHelper is None:
            raise RuntimeError("HGCalCalibrationsDBHelper is not available in this environment")
        print("Pushing JSON to Conditions DB...")
        dbhelper = HGCalCalibrationsDBHelper(
            database="Conditions", table="conditions_test", mysql_env=args.mysql_env
        )
        status = dbhelper.push_json(
            args.output,
            runType="level0",
            reference="0",
            timestamp=datetime.datetime.now(),
        )
        dbhelper.close()
        if status:
            print("JSON successfully pushed to DB")
        else:
            print("Failed to push JSON to DB")

    print(f"All done")


if __name__ == "__main__":
    main()
