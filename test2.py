#!/usr/bin/env python
import subprocess
import sys
import cv2
import numpy as np
import threading
import time

def start_ffmpeg(ip, user, password, channel):
    """Start ffmpeg process for a given RTSP channel."""
    width, height = (704, 576) if channel == 0 else (352, 288)
    command = [
        "ffmpeg",
        "-rtsp_transport", "tcp",
        "-i", f"rtsp://{user}:{password}@{ip}:554/cam/realmonitor?channel={channel}&subtype=0",
        "-f", "image2pipe",
        "-pix_fmt", "bgr24",
        "-vcodec", "rawvideo",
        "-"
    ]
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=10**8
    )
    return process, width, height

def motion_detection(ip, user, password):
    """Detect motion in multiple streams and switch display accordingly."""
    processes = {}
    frame_sizes = {}
    fgbg = {ch: cv2.createBackgroundSubtractorMOG2() for ch in range(7)}
    active_channel = 0

    # Start processes for all channels
    for ch in range(7):
        processes[ch], width, height = start_ffmpeg(ip, user, password, ch)
        frame_sizes[ch] = width * height * 3
    
    while True:
        motion_detected = False
        new_channel = 0

        for ch in range(1, 7):  # Check motion on channels 1-6
            raw_frame = processes[ch].stdout.read(frame_sizes[ch])
            if len(raw_frame) != frame_sizes[ch]:
                continue

            frame = np.frombuffer(raw_frame, dtype='uint8').reshape((frame_sizes[ch] // (3 * 352), 352, 3))
            fgmask = fgbg[ch].apply(frame)
            _, thresh = cv2.threshold(fgmask, 128, 255, cv2.THRESH_BINARY)
            contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            for contour in contours:
                if cv2.contourArea(contour) > 500:
                    new_channel = ch
                    motion_detected = True
                    break
            
            if motion_detected:
                break

        if new_channel != active_channel:
            active_channel = new_channel
            subprocess.run(["pkill", "-f", "ffplay"])
            subprocess.Popen(["ffplay", "-fflags", "nobuffer", f"rtsp://{user}:{password}@{ip}:554/cam/realmonitor?channel={active_channel}&subtype=0"])
        
        time.sleep(1)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: python {sys.argv[0]} <IP> <USER> <PASS>")
        sys.exit(1)
    
    ip, user, password = sys.argv[1:]
    motion_detection(ip, user, password)

