
const char *updateHTML = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <title>Update Home Autoamtion</title>
        <style>
            body { font-family: Arial, sans-serif; margin: 20px; }
            form { margin-top: 20px; }
            .progress { margin-top: 20px; width: 100%; background-color: #f3f3f3; }
            .progress-bar { width: 0%; height: 30px; background-color: #4CAF50; text-align: center; line-height: 30px; color: white; }
        </style>
    </head>
    <body>
        <h1>Firmware Update</h1>
        <form method='POST' action='/update' enctype='multipart/form-data'>
            <input type='file' name='update'>
            <input type='submit' value='Update'>
        </form>
        <div class="progress">
            <div class="progress-bar" id="progress">0%</div>
        </div>
        <script>
            document.querySelector('form').addEventListener('submit', function(e) {
                var form = this;
                var fileInput = form.querySelector('input[type="file"]');
                if (fileInput.files.length > 0) {
                    var xhr = new XMLHttpRequest();
                    var formData = new FormData(form);
                    
                    xhr.upload.addEventListener('progress', function(evt) {
                        if (evt.lengthComputable) {
                            var percentComplete = (evt.loaded / evt.total) * 100;
                            document.getElementById('progress').style.width = percentComplete + '%';
                            document.getElementById('progress').innerHTML = Math.round(percentComplete) + '%';
                        }
                    }, false);
                    
                    xhr.open('POST', '/update', true);
                    xhr.send(formData);
                }
                e.preventDefault();
            });
        </script>
    </body>
    </html>
    )rawliteral";