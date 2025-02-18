#!/usr/bin/env python3
import subprocess
import sys
import cv2
import numpy as np

def start_ffmpeg(ip, user, password, channel):
    """Start ffmpeg process for a given RTSP channel."""
    if channel == 0:
        width, height, subtype = 704, 576, 0
    else:
        width, height, subtype = 1920, 1080, 0
    
    command = [
        "ffmpeg",
        "-rtsp_transport", "tcp",
        "-i", f"rtsp://{user}:{password}@{ip}:554/cam/realmonitor?channel={channel}&subtype={subtype}",
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
    """Detect motion in multiple streams and display accordingly."""
    processes = {}
    frame_sizes = {}
    fgbg = {ch: cv2.createBackgroundSubtractorMOG2() for ch in range(7)}

    # Start processes for all channels
    for ch in range(7):
        processes[ch], width, height = start_ffmpeg(ip, user, password, ch)
        frame_sizes[ch] = width * height * 3
    
    while True:
        display_frame = None

        for ch in range(7):
            raw_frame = processes[ch].stdout.read(frame_sizes[ch])
            if len(raw_frame) != frame_sizes[ch]:
                continue  # Skip incomplete frames

            try:
                frame = np.frombuffer(raw_frame, dtype='uint8')
                frame = frame.reshape((height, width, 3)).copy()
            except ValueError:
                continue  # Skip if reshaping fails
            
            fgmask = fgbg[ch].apply(frame)
            _, thresh = cv2.threshold(fgmask, 128, 255, cv2.THRESH_BINARY)
            contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
            
            for contour in contours:
                if cv2.contourArea(contour) > 1000:
                    (x, y, w, h) = cv2.boundingRect(contour)
                    cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                    display_frame = frame
                    break
            
            if display_frame is not None:
                break

        if display_frame is None:
            raw_frame = processes[0].stdout.read(frame_sizes[0])
            if len(raw_frame) == frame_sizes[0]:
                try:
                    display_frame = np.frombuffer(raw_frame, dtype='uint8').reshape((576, 704, 3)).copy()
                except ValueError:
                    continue  # Skip frame if reshaping fails
        
        resized_frame = cv2.resize(display_frame, (1800, 1000))
        cv2.imshow('Motion Detection', resized_frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    
    for process in processes.values():
        process.terminate()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: python {sys.argv[0]} <IP> <USER> <PASS>")
        sys.exit(1)
    
    ip, user, password = sys.argv[1:]
    motion_detection(ip, user, password)

