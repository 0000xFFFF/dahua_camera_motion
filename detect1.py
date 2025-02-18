#!/usr/bin/env python
import subprocess
import time
import re
import signal
import sys
import cv2
import numpy as np

def start_ffmpeg(ip, user, password):
    """Start ffmpeg and return the process object."""
    command = [
        "ffmpeg",
        "-rtsp_transport", "tcp",
        "-i", f"rtsp://{user}:{password}@{ip}:554/cam/realmonitor?channel=0&subtype=0",
        "-vf", "crop=704:384:0:0",
        "-f", "image2pipe",
        "-pix_fmt", "bgr24",
        "-vcodec", "rawvideo",
        "-"
    ]
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        bufsize=10**8
    )
    return process

def motion_detection(ip, user, password):
    """Detect motion in the video stream and draw rectangles around moving objects."""
    process = start_ffmpeg(ip, user, password)
    width, height = 704, 384  # Adjust based on your camera resolution
    frame_size = width * height * 3

    # Initialize background subtractor
    fgbg = cv2.createBackgroundSubtractorMOG2(history=500, varThreshold=16, detectShadows=True)

    while True:
        # Read a frame from the ffmpeg process
        raw_frame = process.stdout.read(frame_size)
        if len(raw_frame) != frame_size:
            print("Error reading frame from ffmpeg.")
            break

        # Convert the raw frame to a numpy array and make it writable
        frame = np.frombuffer(raw_frame, dtype='uint8').reshape((height, width, 3))
        frame = frame.copy()  # Make the frame writable

        # Apply background subtraction
        fgmask = fgbg.apply(frame)

        # Threshold the mask to get binary image
        _, thresh = cv2.threshold(fgmask, 128, 255, cv2.THRESH_BINARY)

        # Find contours of moving objects
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

        # Draw rectangles around moving objects
        for contour in contours:
            if cv2.contourArea(contour) > 50:  # Filter out small contours
                (x, y, w, h) = cv2.boundingRect(contour)
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 1)
                #cv2.drawContours(frame, [contour], -1, (255, 0, 0), 2) # draw contour

        # Display the frame
        frame = cv2.resize(frame, (1800, 1000)) # resize frame to big
        cv2.imshow('Motion Detection', frame)

        # Break the loop if 'q' is pressed
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    # Release resources
    process.terminate()
    process.wait()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: python {sys.argv[0]} <IP> <USER> <PASS>")
        sys.exit(1)

    ip = sys.argv[1]
    user = sys.argv[2]
    password = sys.argv[3]

    motion_detection(ip, user, password)
