from datetime import datetime, timedelta

import cv2 as cv
import numpy as np
import os

if os.uname()[1] == 'raspberrypi':
    ROOT = '/home/pi/Desktop/traffic_tracker'
    DIR_PHOTOS = f"{ROOT}/photos/"
else:
    ROOT = '/home/joe/prog/c/traffic-tracker/v3/traffic_tracker'
    DIR_PHOTOS = f"{ROOT}/photos/test/"
now = datetime.now()
DIR_PHOTOS += (now - timedelta(days=1)).strftime("%Y-%m-%d")
FN_LOGS = f"{ROOT}/logs/{now.strftime('%Y-%m-%d')}.log"

MIN_MATCH_VAL = 0.30
TEST, LOGGING = False, True
RESIZE = (650, 500)
WHITE = (255, 255, 255)

template = cv.imread(f"{ROOT}/py/template.jpg", cv.IMREAD_GRAYSCALE)
template_width, template_height = template.shape[::-1]
total, cropped, deleted = 0, 0, 0


def match(fn):
    global cropped, deleted
    if TEST:
        print(fn)
    img = cv.imread(fn)
    # (h, w) = img.shape[:2]
    # img = img[0: h, 1100:w]
    img_gray = cv.cvtColor(img, cv.COLOR_BGR2GRAY)
    if np.average(img_gray) < 60:  # SKIP MAINLY BLACK PHOTOS
        os.remove(fn)
        deleted += 1
        if TEST:
            print('Too dark')
        return
    res = cv.matchTemplate(img_gray, template, cv.TM_CCOEFF_NORMED)
    min_val, max_val, min_loc, (x, y) = cv.minMaxLoc(res)
    if max_val > MIN_MATCH_VAL:
        img_save = img[y: y + template_height, x: x + template_width]
        cv.imwrite(fn, img_save)
        cropped += 1
        if TEST:
            print(f"Match: {str(max_val)}")
    else:
        os.remove(fn)
        deleted += 1
        if TEST:
            print(f"No match: {str(max_val)}")
    if TEST:
        cv.rectangle(img, (x, y), (x + template.shape[0], y + template.shape[1]), WHITE, 2)
        txt = 'MATCH' if max_val > MIN_MATCH_VAL else 'No match'
        cv.imshow(txt + ' ' + fn + ': ' + str(max_val), cv.resize(img, RESIZE))
        cv.waitKey(0)
        cv.destroyAllWindows()


def main():
    global total
    os.chdir(DIR_PHOTOS)
    if LOGGING:
        log("INFO", f"Starting script, {os.getcwd()}")
    for fn in os.listdir():
        total += 1
        match(f"{DIR_PHOTOS}/{fn}")
    if LOGGING:
        log("INFO", f"Finished script, cropped+deleted/total: {cropped}+{deleted}/{total}")


def log(loglevel, msg):
    entry = f"{datetime.now().strftime('%H:%M:%S')}, {loglevel} [clean_up_photos.py]: {msg}"
    with open(FN_LOGS, "a+") as f:
        f.write(entry)
        f.write("\n ")


if __name__ == '__main__':
    main()
