#!/usr/bin/env python3
import cv2
import numpy as np

# Define display dimensions
display_width = 1920
display_height = 1080

# Create a window and set it to fullscreen
cv2.namedWindow('Motion', cv2.WINDOW_NORMAL)
cv2.setWindowProperty('Motion', cv2.WND_PROP_FULLSCREEN, cv2.WINDOW_FULLSCREEN)

def get_minimap_position(zoom_rect, frame_dims, minimap_dims, margin=10):
    """Determine the best position for the minimap based on zoom area."""
    zoom_x, zoom_y, zoom_w, zoom_h = zoom_rect
    frame_w, frame_h = frame_dims
    minimap_w, minimap_h = minimap_dims
    
    # Calculate center of zoom area
    zoom_center_x = zoom_x + zoom_w / 2
    zoom_center_y = zoom_y + zoom_h / 2
    
    # Normalize coordinates to 0-1 range
    norm_x = zoom_center_x / frame_w
    norm_y = zoom_center_y / frame_h
    
    # Determine which corner to use (opposite to the action)
    # Divide screen into quadrants
    if norm_x < 0.5:
        if norm_y < 0.5:
            # Action in top-left, place minimap in bottom-right
            pos_x = frame_w - minimap_w - margin
            pos_y = frame_h - minimap_h - margin
        else:
            # Action in bottom-left, place minimap in top-right
            pos_x = frame_w - minimap_w - margin
            pos_y = margin
    else:
        if norm_y < 0.5:
            # Action in top-right, place minimap in bottom-left
            pos_x = margin
            pos_y = frame_h - minimap_h - margin
        else:
            # Action in bottom-right, place minimap in top-left
            pos_x = margin
            pos_y = margin
            
    return int(pos_x), int(pos_y)

def motion_detection(ip, user, password):
    """Detect motion in the RTSP stream using OpenCV and zoom in on motion area."""
    rtsp_url = f"rtsp://{user}:{password}@{ip}:554/cam/realmonitor?channel=0&subtype=0"
    cap = cv2.VideoCapture(rtsp_url)
    if not cap.isOpened():
        print("Error: Could not open RTSP stream.")
        return

    fgbg = cv2.createBackgroundSubtractorMOG2(history=500, varThreshold=16, detectShadows=True)
    zoom_active = False
    zoom_rect = (0, 0, 235, 192)

    # Define minimap size
    minimap_width = 300
    minimap_height = 160


    while True:
        ret, frame = cap.read()
        
        if not ret:
            print("Error: Failed to grab frame.")
            break
        
        # Crop the frame
        crop_width = 704
        crop_height = 384
        crop_x = 0
        crop_y = 0
        frame = frame[crop_y:crop_y + crop_height, crop_x:crop_x + crop_width]
        
        fgmask = fgbg.apply(frame)
        _, thresh = cv2.threshold(fgmask, 128, 255, cv2.THRESH_BINARY)
        contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        
        max_area = 0
        best_rect = None
        for contour in contours:
            if cv2.contourArea(contour) > 10:
                x, y, w, h = cv2.boundingRect(contour)
                if cv2.contourArea(contour) > max_area:
                    max_area = cv2.contourArea(contour)
                    best_rect = (x, y, w, h)
        
        if best_rect:
            x, y, w, h = best_rect
            zoom_x = max(0, x + w//2 - 235//2)
            zoom_y = max(0, y + h//2 - 192//2)
            zoom_rect = (zoom_x, zoom_y, 235, 192)
            zoom_active = True
        else:
            zoom_active = False
        
        # Create display frame
        if zoom_active:
            # Get zoomed portion
            zx, zy, zw, zh = zoom_rect
            zoomed_frame = frame[zy:zy+zh, zx:zx+zw]
            main_display = cv2.resize(zoomed_frame, (display_width, display_height))
            
            # Create minimap
            minimap = cv2.resize(frame, (minimap_width, minimap_height))
            
            # Calculate rectangle position for minimap
            mini_scale_x = minimap_width / crop_width
            mini_scale_y = minimap_height / crop_height
            mini_rect_x = int(zx * mini_scale_x)
            mini_rect_y = int(zy * mini_scale_y)
            mini_rect_w = int(zw * mini_scale_x)
            mini_rect_h = int(zh * mini_scale_y)
            
            # Draw rectangle on minimap
            cv2.rectangle(minimap, 
                         (mini_rect_x, mini_rect_y), 
                         (mini_rect_x + mini_rect_w, mini_rect_y + mini_rect_h), 
                         (0, 255, 0), 
                         1)
            
            # Add padding around minimap
            padding = 2
            minimap_padded = cv2.copyMakeBorder(minimap, 
                                               padding, padding, 
                                               padding, padding, 
                                               cv2.BORDER_CONSTANT, 
                                               value=(255, 255, 255))
            
            # Get dynamic position for minimap
            minimap_x, minimap_y = get_minimap_position(
                zoom_rect,
                (crop_width, crop_height),
                (minimap_padded.shape[1], minimap_padded.shape[0])
            )
            
            # Scale minimap position to display size
            display_minimap_x = int(minimap_x * display_width / crop_width)
            display_minimap_y = int(minimap_y * display_height / crop_height)
            
            # Place minimap on main display
            roi = main_display[display_minimap_y:display_minimap_y+minimap_padded.shape[0], 
                             display_minimap_x:display_minimap_x+minimap_padded.shape[1]]
            
            # Create a mask for the minimap region
            minimap_gray = cv2.cvtColor(minimap_padded, cv2.COLOR_BGR2GRAY)
            _, mask = cv2.threshold(minimap_gray, 1, 255, cv2.THRESH_BINARY)
            
            # Copy minimap to main display
            np.copyto(roi, minimap_padded)
            
        else:
            main_display = cv2.resize(frame, (display_width, display_height))
        
        cv2.imshow('Motion', main_display)
        
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 4:
        print(f"Usage: python {sys.argv[0]} <IP> <USER> <PASS>")
        sys.exit(1)
    
    motion_detection(sys.argv[1], sys.argv[2], sys.argv[3])
