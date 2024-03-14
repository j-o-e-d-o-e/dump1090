from datetime import datetime, timedelta

import cv2 as cv
import numpy as np
import os

if os.uname()[1] == 'raspberrypi':
    DIR_PHOTOS = '/home/pi/Desktop/flight_tracker/photos/'
else:
    DIR_PHOTOS = '/home/joe/prog/c/traffic-tracker/v3/traffic_tracker/photos/test/'
today = datetime.now()
yesterday = today - timedelta(days=1)
DIR_PHOTOS += yesterday.strftime("%Y-%m-%d") + '/'
MIN_MATCH_VAL = 0.30
RESIZE = (650, 500)
WHITE = (255, 255, 255)


def match(fn):
    img = cv.imread(DIR_PHOTOS + fn)
    # (h, w) = img.shape[:2]
    # img = img[0: h, 1100:w]
    img_gray = cv.cvtColor(img, cv.COLOR_BGR2GRAY)
    if np.average(img_gray) < 60:  # SKIP MAINLY BLACK PHOTOS
        os.remove(DIR_PHOTOS + fn)
        return
    template = cv.imread('template.jpg', cv.IMREAD_GRAYSCALE)
    width_temp, height_temp = template.shape[::-1]
    res = cv.matchTemplate(img_gray, template, cv.TM_CCOEFF_NORMED)
    min_val, max_val, min_loc, (x, y) = cv.minMaxLoc(res)
    if max_val > MIN_MATCH_VAL:
        img_save = img[y: y + height_temp, x: x + width_temp]
        cv.imwrite(DIR_PHOTOS + fn, img_save)
    else:
        os.remove(DIR_PHOTOS + fn)


def main():
    for i, fn in enumerate(os.listdir(DIR_PHOTOS)):
        match(fn)


if __name__ == '__main__':
    main()