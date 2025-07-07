Import("env")

def upload_all_action(*args, **kwargs):
    # Mevcut environment'ı al (örn. main-lynk)
    env_name = env["PIOENV"]
    
    # upload ve uploadfs hedeflerini sırayla çalıştır
    env.Execute(f"pio run -e {env_name} --target upload")
    env.Execute(f"pio run -e {env_name} --target uploadfs")

env.AddCustomTarget(
    name="uploadall",
    dependencies=None,
    actions=[upload_all_action],
    title="Upload Firmware and Filesystem",
    description="Tek seferde hem program kodunu hem de SPIFFS imajını yükler."
)
