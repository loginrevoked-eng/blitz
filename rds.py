import redis

r = redis.Redis(
    host='38.180.138.150',
    port=6379,
    username='default',        # Replace if you have a specific ACL username; omit if using legacy password-only
    password='o5j4w56p42ittftu!',
    decode_responses=True,
    protocol=2
)

# Safely stream all key names without blocking the server
for key in r.scan_iter():
    print(key)
