<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd"> 
	<html lang="en"> 
		<head> 
		<meta http-equiv="content-type" content="text/html; charset=utf-8">

		<script type="text/javascript">
			function crypt() {
				var mssg = "The quick brown fox jumps over the lazy dog";
				var arr = mssg.split("");
				console.log(arr);

				window.crypto.subtle.digest({
						name: "SHA-256",
				},
				new Uint8Array(arr) //The data you want to hash as an ArrayBuffer
				).then(function(hash){
					console.log(new Uint8Array(hash));
				}).catch(function(err){
					console.error(err);
				}); 
			}
	window.onload = crypt;
</script>

<title>Crypto!</title>
	</head>
	<body> 
		<p>
			This does cool stuff
		</p>
	</body>
</html>

