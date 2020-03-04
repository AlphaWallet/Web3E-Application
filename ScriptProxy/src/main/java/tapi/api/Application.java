package tapi.api;

import org.springframework.boot.CommandLineRunner;
import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;

@SpringBootApplication
//@EnableConfigurationProperties(StorageProperties.class)
public class Application {

	public static void main(String[] args)
	{
		SpringApplication.run(Application.class, args);
	}

	@Bean
	CommandLineRunner init() {
		return (args) -> {

		};
	}
}